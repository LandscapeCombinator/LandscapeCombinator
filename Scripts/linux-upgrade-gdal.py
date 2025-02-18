import os
import urllib.request
import sys

geos_url = 'https://github.com/libgeos/geos/releases/download/3.13.0/geos-3.13.0.tar.bz2'
geos_dir = os.path.abspath('geos-3.13.0')

gdal_url = 'https://github.com/OSGeo/gdal/releases/download/v3.10.1/gdal-3.10.1.tar.gz'
gdal_dir = os.path.abspath('gdal-3.10.1')

plugin_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))

if len(sys.argv) < 3:
    print('First argument: Engine root, Second argument: target directory')
    sys.exit(1)

engine_root = sys.argv[1]
target_dir = sys.argv[2]
rocky = os.path.join(engine_root, 'Extras/ThirdPartyNotUE/SDKs/HostLinux/Linux_x64/v23_clang-18.1.0-rockylinux8/x86_64-unknown-linux-gnu')

cc = os.path.join(rocky, 'bin/clang')
cxx = os.path.join(rocky, 'bin/clang++')
# ld = os.path.join(rocky, 'bin/ld.lld')
lib64 = os.path.join(rocky, 'usr/lib64')

libcxx_include = os.path.join(engine_root, 'Source/ThirdParty/Unix/LibCxx/include')
libcxx = os.path.join(engine_root, 'Source/ThirdParty/Unix/LibCxx/lib/Unix/x86_64-unknown-linux-gnu/libc++.a')
libcxx_abi = os.path.join(engine_root, 'Source/ThirdParty/Unix/LibCxx/lib/Unix/x86_64-unknown-linux-gnu/libc++abi.a')

# geos_lib = os.path.join(geos_prefix, 'lib/libgeos_c.so') # dynamic library because LGPL

to_check = [
    cc, cxx,
    # ld,
    lib64,
    libcxx_include, libcxx, libcxx_abi
]

for path in to_check:
    if not os.path.exists(path):
        print(f'Path {path} does not exist')
        sys.exit(1)

# lib64_all = [os.path.join(lib64, f) for f in os.listdir(lib64) if f.endswith('.a')]
# print(f'Found libraries: {" ".join(lib64_all)}')
common_flags = f'--sysroot={rocky} -B={rocky}/usr/lib64'
compiler_flags = f'{common_flags}'
cxx_flags = f'{common_flags} -nostdinc++ -std=c++11 -I{libcxx_include} -I{libcxx_include}/c++/v1'
linker_flags = f'{common_flags} {libcxx} {libcxx_abi} -L={rocky}/usr/lib64 -fuse-ld=lld -nodefaultlibs -lpthread -lm -ldl -lc -lgcc_s -lgcc'

def build_geos():
    filename = geos_url.split("/")[-1]
    if not os.path.exists(filename):
        print(f'Downloading GEOS into {filename}...')
        urllib.request.urlretrieve(geos_url, filename)

    if not os.path.exists(geos_dir):
        print('Extracting GEOS...')
        os.system(f'tar -xf {filename}')

    print('Building GEOS...')
    os.chdir(geos_dir)
    
    if not os.path.exists('build'):
        os.mkdir('build')
    os.chdir('build')
    cmake_args = [
        'cmake',
        '-DCMAKE_POSITION_INDEPENDENT_CODE=ON',
        '-DBUILD_SHARED_LIBS=ON',
        f'-DCMAKE_C_COMPILER={cc}',
        f'-DCMAKE_CXX_COMPILER={cxx}',
        f'-DCMAKE_CXX_FLAGS="{cxx_flags}"',
        # f'-DCMAKE_CXX_LINK_EXECUTABLE={ld}',
        f'-DCMAKE_EXE_LINKER_FLAGS="{linker_flags}"',
        f'-DCMAKE_SHARED_LINKER_FLAGS="{linker_flags}"',
        '-DCMAKE_BUILD_TYPE=Release',
        '..'
    ]
    cmake_cmd = ' '.join(cmake_args)
    print(cmake_cmd)
    os.system(cmake_cmd)
    os.system('cmake --build . --target geos -j32')
    os.system('cmake --build . --target geos_c -j32')

geos_source_include = f'{geos_dir}/include'
geos_include = f'{geos_dir}/build/capi'
openssl_include = os.path.join(engine_root, 'Source/ThirdParty/OpenSSL/1.1.1t/include/Unix')

proj_lib = os.path.join(engine_root, 'Plugins/Runtime/GeoReferencing/Source/ThirdParty/vcpkg-installed/overlay-x64-linux/lib/libproj.a')
sqlite_lib = os.path.join(engine_root, 'Plugins/Runtime/GeoReferencing/Source/ThirdParty/vcpkg-installed/overlay-x64-linux/lib/libsqlite3.a')
proj_include = os.path.join(engine_root, 'Plugins/Runtime/GeoReferencing/Source/ThirdParty/vcpkg-installed/overlay-x64-linux/include')

expat_lib = os.path.join(engine_root, 'Source/ThirdParty/Expat/expat-2.2.10/Unix/x86_64-unknown-linux-gnu/Release/libexpat.a')
expat_include = os.path.join(engine_root, 'Source/ThirdParty/Expat/expat-2.2.10/lib')

# system installed static libraries
webp_lib = os.path.join(plugin_dir, 'Source/ThirdParty/libwebp/lib/libwebp.a')
webp_include = os.path.join(plugin_dir, 'Source/ThirdParty/libwebp/include')

libsharpyuv = os.path.join(plugin_dir, 'Source/ThirdParty/libwebp/lib/libsharpyuv.a')

libgeos = os.path.join(geos_dir, 'build/lib/libgeos.so')
libgeos_c = os.path.join(geos_dir, 'build/lib/libgeos_c.so')

to_check2 = [
    geos_source_include, geos_include, libgeos, libgeos_c, openssl_include,
    proj_lib, sqlite_lib, proj_include,
    expat_lib, expat_include,
    webp_lib, webp_include, libsharpyuv
]

gdal_compiler_flags=f'-I{openssl_include} -I{geos_include} -I{geos_source_include} -I{webp_include}'
gdal_linker_flags=f'{libsharpyuv} -L{geos_dir}/build/lib -lgeos_c -lgeos'

def build_gdal():
    filename = gdal_url.split("/")[-1]
    if not os.path.exists(filename):
        print(f'Downloading GDAL into {filename}...')
        urllib.request.urlretrieve(gdal_url, filename)
        
    if not os.path.exists(gdal_dir):
        print('Extracting GDAL...')
        os.system(f'tar -xf {filename}')

    print('Building GDAL...')
    os.chdir(gdal_dir)
    if not os.path.exists('build'):
        os.mkdir('build')
    os.chdir('build')
    
    # use proj_lib instead of geos_lib for DGEOS_LIBRARY, as a workaround because GDAL build
    # expects a static library, while geos_lib is dynamically linked
    cmake_args = [
        'cmake',
        '-G "Ninja"',
        # '-DCMAKE_VERBOSE_MAKEFILE=ON',
        '-DCMAKE_POSITION_INDEPENDENT_CODE=ON',
        '-DBUILD_SHARED_LIBS=ON',
        '-D_TEST_LINK_STDCPP=TRUE', # hack to skip failing cmake test
        f'-DCMAKE_C_COMPILER={cc}',
        f'-DCMAKE_C_FLAGS="{common_flags} {gdal_compiler_flags}"',
        f'-DCMAKE_CXX_COMPILER={cxx}',
        f'-DCMAKE_CXX_FLAGS="{cxx_flags} {gdal_compiler_flags}"',
        # f'-DCMAKE_CXX_LINK_EXECUTABLE={ld}',
        f'-DCMAKE_EXE_LINKER_FLAGS="{linker_flags} {gdal_linker_flags}"',
        f'-DCMAKE_SHARED_LINKER_FLAGS="{linker_flags} {gdal_linker_flags}"',
        f'-DCMAKE_MODULE_LINKER_FLAGS="{linker_flags} {gdal_linker_flags}"',
        '-DCMAKE_BUILD_TYPE=Release',
        f'-DPROJ_LIBRARY={proj_lib}',
        f'-DPROJ_INCLUDE_DIR={proj_include}',
        '-DGDAL_USE_INTERNAL_LIBS=ON',
        '-DGDAL_USE_EXTERNAL_LIBS=OFF',
        '-DGDAL_USE_EXPAT=ON',
        f'-DEXPAT_LIBRARY={expat_lib}',
        f'-DEXPAT_INCLUDE_DIR={expat_include}',
        '-DEXPAT_USE_STATIC_LIBS=ON',
        '-DGDAL_USE_OPENSSL=ON',
        '-DOPENSSL_USE_STATIC_LIBS=TRUE',
        '-DOPENSSL_MSVC_STATIC_RT=TRUE',
        '-DGDAL_USE_WEBP=ON',
        f'-DWEBP_LIBRARY={webp_lib}',
        f'-DWEBP_INCLUDE_DIR={webp_include}',
        '-DGDAL_USE_SQLITE3=ON',
        f'-DSQLite3_LIBRARY={sqlite_lib}',
        f'-DSQLite3_INCLUDE_DIR={proj_include}', # same directory as proj
        '-DACCEPT_MISSING_SQLITE3_RTREE:BOOL=ON',
        '-DACCEPT_MISSING_SQLITE3_MUTEX_ALLOC:BOOL=ON',
        '-DGDAL_USE_GEOS=ON',
        f'-DGEOS_INCLUDE_DIR={geos_include}',
        f'-DGEOS_LIBRARY={proj_lib}',
        '..'
    ]
    cmd = ' '.join(cmake_args)
    os.system(cmd)
    os.system(f'cmake --build . -j32')

# build_geos()
# os.chdir('../..')

# for path in to_check2:
#     if not os.path.exists(path):
#         print(f'Path {path} does not exist')
#         sys.exit(1)

# build_gdal()

os.system(f'cp -a {libgeos}* {target_dir}')
os.system(f'cp -a {libgeos_c}* {target_dir}')

for lib in ['libpthread.so.0', 'libm.so.6', 'libdl.so.2', 'libc.so.6']:
    source = os.path.join(rocky, 'lib64', lib)
    os.system(f'cp {source} {target_dir}')

os.system(f'cp -a {os.path.join(gdal_dir, "build/libgdal.so*")} {target_dir}')
