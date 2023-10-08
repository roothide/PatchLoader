#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>
#include <assert.h>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#include <sys/syslimits.h>

int64_t (*jbdswDebugMe)(void)=NULL;

typedef void (*PatchLoader)(const char* path, const struct mach_header *header, intptr_t slide);

static void image_load_handler(const char* path, const struct mach_header *header, intptr_t slide)
{
    char patcher[PATH_MAX]={0};
    snprintf(patcher,sizeof(patcher),"%s.roothidepatch",path);
    if(access(patcher, F_OK) == 0) 
    {
        assert(jbdswDebugMe() == 0);

        void* handler = dlopen(patcher, RTLD_NOW);
        assert(handler != NULL);
        
        void* patchloader = dlsym(handler, "PatchLoader");
        assert(patchloader != NULL);

        ((PatchLoader)patchloader)(path,header,slide);
    }
}

static void image_load_callback(const struct mach_header *header, intptr_t slide)
{
    int c=_dyld_image_count();
    for(int i=0; i<c; i++) {
        if(header == _dyld_get_image_header(i)) {
            const char* path = _dyld_get_image_name(i);
            if(!_dyld_shared_cache_contains_path(path))
                image_load_handler(path, header, slide);
            break;
        }
    }
}

static void __attribute__((constructor)) initializer()
{
    *(void**)&jbdswDebugMe = dlsym(RTLD_DEFAULT, "jbdswDebugMe");

    _dyld_register_func_for_add_image(image_load_callback);
}
