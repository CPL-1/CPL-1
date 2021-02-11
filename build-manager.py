import fire
import os
import json
from pathlib import Path

# Create list of all applications that are supported
def generate_program_list_for_arch(arch: str):
    result = []
    for filename in os.listdir('userspace'):
        full_filename = str(Path('userspace')/filename)
        if not (Path('userspace')/filename/'build'/arch).is_dir():
            print(f"Skipping {full_filename}: {filename} program has no support for architecture {arch}")
            continue
        result.append(filename)
    return result

# Check that architecture is supported by all core components
# (that is kernel, userlib, init, and image creation)
def is_arch_supported(arch: str):
    # Kernel
    if not (Path('kernel')/'build'/arch).is_dir():
        return False
    # User library
    if not (Path('userlib')/'build'/arch).is_dir():
        return False
    # Init
    if not (Path('init')/'build'/arch).is_dir():
        return False
    # Image creation
    if not (Path('build')/arch).is_dir():
        return False
    return True

# Get list of all architectures supported
# TODO: Perhaps add some kind of a manifest for arches here?
def generate_all_arches_list():
    arches = []
    # Iterate over all makefiles for kernel
    for filename in os.listdir(str(Path('kernel')/'build')):
        # Check that arch is supported
        if is_arch_supported(filename):
           arches.append(filename)
    return arches

# Generate list of all architectures specified by arch_arg
def generate_arch_list(arch_arg):
    if arch_arg == 'all':
        return generate_all_arches_list()
    elif type(arch_arg) == str:
        return [arch_arg]
    elif type(arch_arg) == list:
        # Validate that all list items are strings
        for arch in arch_arg:
            if type(arch) != 'str':
                return None
        return arch_arg
    else:
        return None

# Generate build config for a given architecture
def generate_system_cfg(arch_arg):
    cfg = {}
    arches = generate_arch_list(arch_arg)
    if arches == None:
        return None
    for arch_name in arches:
        cfg[arch_name] = {'programs': generate_program_list_for_arch(arch_name)}
    return cfg

# Execute array of commands
def execute_array(arr):
    for cmd in arr:
        os.system(cmd)

# Create directory if does not exist
def create_dir(path):
    try:
        os.mkdir(path)
    except FileExistsError:
        pass

# Build submodule
def build_submodule(path, src, out, dst, mode, vars=''):
    cmd = f'make -C {str(path)} {mode} {vars}; cp {str(path/src)} {str(out/dst)}'
    print(f'Executing \"{cmd}\"...')
    build_result = os.system(cmd)
    return build_result == 0

# Clean submodule
def clean_submodule(path):
    cmd = f'make -C {str(path)} clean'
    print(f'Executing \"{cmd}\"...')
    os.system(cmd)

# Build system for an architecture
def build_arch(arch, mode, programs):
    # Create filesystem root
    out_dir_path = Path('out')/arch
    create_dir(str(out_dir_path)) # Out root for this arch
    os.system(f'cp -r {str(Path("root")/"*")} {str(out_dir_path)}')
    create_dir(str(out_dir_path/'boot')) # Directory for the kernel
    create_dir(str(out_dir_path/'bin')) # Directory for programs
    create_dir(str(out_dir_path/'sbin')) # Directory for system utilities
    create_dir(str(out_dir_path/'usr')) # Directory for usr things
    create_dir(str(out_dir_path/'dev')) # Directory for device fs mount
    create_dir(str(out_dir_path/'usr'/'lib')) # Directory for libraries
    # Start with building kernel
    print("Building kernel...")
    kernel_path = Path('kernel')/'build'/arch
    if not build_submodule(kernel_path, f'kernel-{mode}.elf', out_dir_path/'boot', 'kernel.elf', mode):
        print(f'Building kernel for architecture {arch} with mode {mode} failed')
        return False
    print("Building kernel done!")
    # Build user library
    print("Building user library...")
    userlib_path = Path('userlib')/'build'/arch
    if not build_submodule(userlib_path, f'libcpl1-{mode}.a', out_dir_path/'usr'/'lib', 'libcpl1.a', mode):
        print(f'Building kernel for architecture {arch} with mode {mode} failed')
        return False
    print("Building user library done!")
    userlib_out_init_path = Path('..')/'..'/'..'/'out'/arch/'usr'/'lib'/'libcpl1.a'
    userlib_out_init_include_path = Path('..')/'..'/'..'/'userlib'/'include'
    userlib_out_path = Path('..')/'..'/'..'/'..'/'out'/arch/'usr'/'lib'/'libcpl1.a'
    userlib_out_include_path = Path('..')/'..'/'..'/'..'/'userlib'/'include'
    # Build init
    print("Building init...")
    userlib_path = Path('init')/'build'/arch
    if not build_submodule(userlib_path, f'init-{mode}', out_dir_path/'sbin', 'init', mode, f'USERLIB={userlib_out_init_path} USERLIB_INCLUDE={userlib_out_init_include_path}'):
        print(f'Building init for architecture {arch} with mode {mode} failed')
        return False
    print("Building user library done!")
    # Build programs
    for program in programs:
        print(f"Building {program}...")
        program_path = Path('userspace')/program/'build'/arch
        if not build_submodule(program_path, f'{program}-{mode}.elf', out_dir_path/'bin', program, mode, f'USERLIB={userlib_out_path} USERLIB_INCLUDE={userlib_out_include_path}'):
            print(f'Building {program} for architecture {arch} with mode {mode} failed')
            return False
    # Build image
    image_cmd = f'make -C {str(Path("build")/arch)} build'
    print('Creating bootable disk image.')
    print(f'Executing command \"{image_cmd}\"')
    if not os.system(image_cmd):
        return False
    return True


def clean_arch(arch):
    # Delete out
    out_dir_path = Path('out')/arch
    os.system(f'rm -rf {str(out_dir_path)}') # Delete if present
    # Clean kernel
    print("Cleaning up kernel folder...")
    kernel_path = Path('kernel')/'build'/arch
    clean_submodule(kernel_path)
    # Clean userlib
    print("Cleaning up userlib folder...")
    userlib_path = Path('userlib')/'build'/arch
    clean_submodule(userlib_path)
    # Clean init
    print("Cleaning up init folder...")
    init_path = Path('init')/'build'/arch
    clean_submodule(init_path)
    # Clean programs (including ones that were not configured if config changed)
    for program in os.listdir('userspace'):
        print(f"Cleaning up {program} folder...")
        program_path = Path('userspace')/program/'build'/arch
        if not program_path.is_dir():
            continue
        clean_submodule(program_path)
    # Clean image folder
    image_cmd = f'make -C {str(Path("build")/arch)} clean'
    print('Cleaning image folder')
    print(f'Executing command \"{image_cmd}\"')
    os.system(image_cmd)
    return True

# Run image for the architecture
def run_arch(arch):
    image_cmd = f'make -C {str(Path("build")/arch)} run'
    print(f'Executing command \"{image_cmd}\"')
    os.system(image_cmd)

class BuildManager:
    def configure(self, arch='all'):
        # Try to generate config
        config = generate_system_cfg(arch)
        if config == None:
            print(f'Failed to generate system config for {repr(arch)}')
            exit(-1)
        # Write config
        path = 'build.cfg'
        try:
            with open(path, 'w') as cfg:
                cfg.write(json.dumps(config, indent=4)) 
        except:
            print(f'Failed to write build config to {path}')
            exit(-1)
        print('Done')
    
    def build(self, arch='configured', mode='release'):
        cfg = {}
        # Read config
        path = 'build.cfg'
        try:
            with open(path, 'r') as cfg_file:
                cfg = json.loads(cfg_file.read())
        except:
            print(f'Failed to read config from {path}')
            exit(-1)
        # Get architectures list
        arch_list = []
        if arch == 'configured':
            arch_list = list(cfg.keys())
        else:
            arch_list = generate_arch_list(arch);
        # Check that all arches are present in config
        for arch in arch_list:
            if arch not in cfg:
                print(f'Architecture {arch} was not configured. Run subcommand config {arch}')
                exit(-1)
        # Check build mode
        if not (mode == 'release' or mode == 'debug'):
            print(f'No build mode {mode}')
            exit(-1)
        # Create out directory
        create_dir('out')
        # Run build process for all arches
        for arch in arch_list:
            if 'programs' not in cfg[arch]:
                print(f"No programs field in config for architecture {arch}")
                exit(-1)
            build_arch(arch, mode, cfg[arch]['programs'])
    
    def clean(self, arch='all'):
        arches = generate_arch_list(arch)
        for arch_name in arches:
            clean_arch(arch_name)
    
    def run(self, arch):
        if type(arch) != str:
            exit(-1)
        if not is_arch_supported(arch):
            print(f"Architecture {arch} is not supported")
            exit(-1)
        run_arch(arch)

fire.Fire(BuildManager())
