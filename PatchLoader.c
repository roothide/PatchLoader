#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>
#include <assert.h>
#include <sys/stat.h>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#include <sys/syslimits.h>
#include <roothide.h>


#define SIGABRT 6
#define OS_REASON_SIGNAL        2
#define OS_REASON_DYLD          6
#define DYLD_EXIT_REASON_OTHER                  9

void abort_with_payload(uint32_t reason_namespace, uint64_t reason_code, void *payload, uint32_t payload_size, const char *reason_string, uint64_t reason_flags) __attribute__((noreturn, cold));

#define	ABORT_WITH(...)	do { \
            char* pinfo=NULL; \
            asprintf(&pinfo, __VA_ARGS__); \
            abort_with_payload(OS_REASON_DYLD,DYLD_EXIT_REASON_OTHER,NULL,0, pinfo, 0); \
            free(pinfo); \
        } while(0)

#if DEBUG == 1
#include <sys/syslog.h>
#define SYSLOG(...) {openlog("roothide",LOG_PID,LOG_AUTH);syslog(LOG_DEBUG, __VA_ARGS__);closelog();}
#else
#define SYSLOG(...)
#endif

extern struct mach_header_64* _dyld_get_prog_image_header();
extern const void* _dyld_get_shared_cache_range(size_t* length);
extern const char* dyld_image_path_containing_address(const void* addr);

int64_t (*PLRequiredJIT)(void) = NULL;

typedef void (*InitPatches)(const char *path, const struct mach_header *header, intptr_t slide);

static void image_load_handler(const char *path, const struct mach_header *header, intptr_t slide)
{
    char patcher[PATH_MAX] = {0};
    snprintf(patcher, sizeof(patcher), "%s.roothidepatch", path);
    struct stat st;
    if (lstat(patcher, &st) == 0)
    {
        SYSLOG("PatchLoader: load patcher %s\n", patcher);

        struct stat st={0};
        if(lstat(jbroot("/var/jb"), &st)!=0 || !S_ISLNK(st.st_mode)) {
            const char* prog_image_path = dyld_image_path_containing_address(_dyld_get_prog_image_header());
            if(strstr(prog_image_path, ".app/")) {
                ABORT_WITH("PatchLoader: jbroot:/var/jb is broken! try reinstalling PatchLoader package.");
                return;
            }
        }

        assert(PLRequiredJIT() == 0);

        void *handler = dlopen(patcher, RTLD_NOW);
        if(!handler) {
            ABORT_WITH("PatchLoader: dlopen %s : %s", patcher, dlerror());
            return;
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

size_t dsclen=0;
uint64_t dscaddr=0;
bool load_init=false;

static void image_load_callback(const struct mach_header *header, intptr_t slide)
{
    if(!load_init) return;

    if((uint64_t)header >= dscaddr && (uint64_t)header < dscaddr + dsclen) {
        return;
    }

    const char* path = dyld_image_path_containing_address(header);
    SYSLOG("PatchLoader: new image %p %p %s\n", header, (void *)slide, path);
    image_load_handler(path, header, slide);
}

static void __attribute__((constructor)) initializer()
{
    *(void **)&PLRequiredJIT = dlsym(RTLD_DEFAULT, "PLRequiredJIT");
    if(!PLRequiredJIT) {
        *(void **)&PLRequiredJIT = dlsym(RTLD_DEFAULT, "jbdswDebugMe");
    }

    if(!PLRequiredJIT) {
        ABORT_WITH("PatchLoader: cannot find PLRequiredJIT symbol!");
        return;
    }

    _dyld_register_func_for_add_image(image_load_callback);

    // dlopen during register may break loaded vector (dyld didn't really lock the vector)

    dsclen = 0;
    dscaddr = (uint64_t)_dyld_get_shared_cache_range(&dsclen);
    assert(dscaddr != 0);
    assert(dsclen != 0);

    int count = _dyld_image_count();
    for (int i = 0; i < count; i++)
    {
        void* header = (void*)_dyld_get_image_header(i);
        
        if((uint64_t)header >= dscaddr && (uint64_t)header < dscaddr + dsclen) {
            continue;
        }

        uint64_t slide = _dyld_get_image_vmaddr_slide(i);
        const char *path = _dyld_get_image_name(i);
        SYSLOG("PatchLoader: loaded image %p %p %s\n", header, (void *)slide, path);
        image_load_handler(path, header, slide);
    }

    load_init=true;
}
