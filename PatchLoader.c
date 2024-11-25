#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>
#include <assert.h>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#include <sys/syslimits.h>


#define SIGABRT 6
#define OS_REASON_SIGNAL        2
#define OS_REASON_DYLD          6
#define DYLD_EXIT_REASON_OTHER                  9

void abort_with_payload(uint32_t reason_namespace, uint64_t reason_code, void *payload, uint32_t payload_size, const char *reason_string, uint64_t reason_flags) __attribute__((noreturn, cold));

#define	ABORT_WITH(e)	abort_with_payload(OS_REASON_DYLD,DYLD_EXIT_REASON_OTHER,NULL,0, e, 0)


#if DEBUG == 1
#include <sys/syslog.h>
#define SYSLOG(...) {openlog("roothide",LOG_PID,LOG_AUTH);syslog(LOG_DEBUG, __VA_ARGS__);closelog();}
#else
#define SYSLOG(...)
#endif

int64_t (*PLRequiredJIT)(void) = NULL;

typedef void (*InitPatches)(const char *path, const struct mach_header *header, intptr_t slide);

static void image_load_handler(const char *path, const struct mach_header *header, intptr_t slide)
{
    char patcher[PATH_MAX] = {0};
    snprintf(patcher, sizeof(patcher), "%s.roothidepatch", path);
    if (access(patcher, F_OK) == 0)
    {            
        SYSLOG("PatchLoader: load patcher %s\n", patcher);

        assert(PLRequiredJIT() == 0);

        void *handler = dlopen(patcher, RTLD_NOW);
        if(!handler) {
            static char buf[4096]={0};
            snprintf(buf,sizeof(buf),"dlopen %s : %s",patcher, dlerror());
            ABORT_WITH(buf);
        }

        void *initpatches = dlsym(handler, "InitPatches");
        assert(initpatches != NULL);

        ((InitPatches)initpatches)(path, header, slide);
    }
/*
    /Library/dpkg/tmp.ci/: preinst  extrainst_  
    /Library/dpkg/info/: postinst prerm postrm 

    /var/mobile/Library/pkgmirror/DEBIAN.
*/
}

bool load_init=false;

static void image_load_callback(const struct mach_header *header, intptr_t slide)
{
    if(!load_init) return;

    int count = _dyld_image_count();
    for (int i = 0; i < count; i++)
    {
        if (header == _dyld_get_image_header(i))
        {
            const char *path = _dyld_get_image_name(i);
            if (!_dyld_shared_cache_contains_path(path)) {
                SYSLOG("PatchLoader: new image %p %p %s\n", header, (void *)slide, path);
                image_load_handler(path, header, slide);
            }
            break;
        }
    }
}

// int proc_regionfilename(int pid, void* address, void * buffer, uint32_t buffersize);

// static void image_load_callback(const struct mach_header *header, intptr_t slide)
// {
//     char path[PATH_MAX]={0};
//     if(proc_regionfilename(getpid(), (void*)header, path, sizeof(path)) > 0)
//     {
//         SYSLOG("PatchLoader: image-load %p %p %s\n", header, (void *)slide, path);
//         if(!_dyld_shared_cache_contains_path(path))
//             image_load_handler(path, header, slide);
//     }
// }

static void __attribute__((constructor)) initializer()
{
    *(void **)&PLRequiredJIT = dlsym(RTLD_DEFAULT, "PLRequiredJIT");
    if(!PLRequiredJIT) *(void **)&PLRequiredJIT = dlsym(RTLD_DEFAULT, "jbdswDebugMe");

    _dyld_register_func_for_add_image(image_load_callback);

    // dlopen during register may break loaded vector (dyld didn't really lock the vector)

    int count = _dyld_image_count();
    for (int i = 0; i < count; i++)
    {
        void* header = (void*)_dyld_get_image_header(i);
        uint64_t slide = _dyld_get_image_vmaddr_slide(i);
        const char *path = _dyld_get_image_name(i);
        if (!_dyld_shared_cache_contains_path(path)) {
            SYSLOG("PatchLoader: loaded image %p %p %s\n", header, (void *)slide, path);
            image_load_handler(path, header, slide);
        }
    }

    load_init=true;
}
