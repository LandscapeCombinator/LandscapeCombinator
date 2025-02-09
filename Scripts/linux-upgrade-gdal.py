import os
import shutil
import tarfile
import urllib.request
import sys

gdal_url = 'https://github.com/OSGeo/gdal/releases/download/v3.10.1/gdal-3.10.1.tar.gz'
gdal_dir = 'gdal-3.10.1'

if len(sys.argv) < 2:
    print('Please provide custom-clang root as an argument')
    sys.exit(1)

clang_root = sys.argv[1]
cc = os.path.join(clang_root, 'conan-ue4cli-clang', 'clang.py')
cxx = os.path.join(clang_root, 'conan-ue4cli-clang', 'clang++.py')
proj_lib = os.path.join(clang_root, 'libproj', 'lib', 'libproj.a')
sqlite_lib = os.path.join(clang_root, 'libproj', 'lib', 'libsqlite3.a')
proj_include = os.path.join(clang_root, 'libproj', 'include')
webp_lib = os.path.join(clang_root, 'libwebp', 'lib', 'libwebp.a')
webp_include = os.path.join(clang_root, 'libwebp', 'include')
expat_lib = os.path.join(clang_root, 'libexpat', 'lib', 'libexpat.a')
expat_include = os.path.join(clang_root, 'libexpat', 'include')
geos_prefix = os.path.join(clang_root, 'libgeos')
geos_lib = os.path.join(geos_prefix, 'lib', 'libgeos_c.so') # dynamic library because LGPL
geos_include = os.path.join(geos_prefix, 'include')

to_check = [cc, cxx, proj_lib, sqlite_lib, proj_include, webp_lib, webp_include, expat_lib, expat_include, geos_prefix, geos_lib, geos_include]

for path in to_check:
    if not os.path.exists(path):
        print(f'Path {path} does not exist')
        sys.exit(1)

def build_gdal():
    filename = gdal_url.split("/")[-1]
    if not os.path.exists(filename):
        print(f'Downloading GDAL into {filename}...')
        urllib.request.urlretrieve(gdal_url, filename)
    print('Extracting GDAL...')
    os.system(f'tar -xzf {filename}')

    print('Building GDAL...')
    os.chdir(gdal_dir)
    if not os.path.exists('build'):
        os.mkdir('build')
    os.chdir('build')
    
    # use proj_lib instead of geos_lib for DGEOS_LIBRARY, as a workaround because GDAL build
    # expects a static library, while geos_lib is dynamically linked
    cmd = f'LDFLAGS=LINKINVOCATION CC={cc} CXX={cxx} cmake ' \
          f'-DPROJ_LIBRARY={proj_lib} ' \
          f'-DPROJ_INCLUDE_DIR={proj_include} ' \
          f'-DGDAL_USE_INTERNAL_LIBS=ON ' \
          f'-DGDAL_USE_EXTERNAL_LIBS=OFF ' \
          f'-DGDAL_USE_EXPAT=ON ' \
          f'-DEXPAT_LIBRARY={expat_lib} ' \
          f'-DEXPAT_INCLUDE_DIR={expat_include} ' \
          f'-DEXPAT_USE_STATIC_LIBS=ON ' \
          f'-DGDAL_USE_OPENSSL=ON ' \
          f'-DGDAL_USE_WEBP=ON ' \
          f'-DGDAL_USE_SQLITE3=ON ' \
          f'-DSQLite3_LIBRARY={sqlite_lib} ' \
          f'-DSQLite3_INCLUDE_DIR={proj_include} ' \
          f'-DACCEPT_MISSING_SQLITE3_RTREE:BOOL=ON ' \
          f'-DWEBP_LIBRARY={webp_lib} ' \
          f'-DWEBP_INCLUDE_DIR={webp_include} ' \
          f'-DGDAL_USE_GEOS=ON ' \
          f'-DGEOS_INCLUDE_DIR={geos_include} ' \
          f'-DGEOS_PREFIX={geos_prefix} ' \
          f'-DGEOS_LIBRARY={proj_lib} ' \
          f'..'
    os.system(cmd)
    os.system(f'LDFLAGS=LINKINVOCATION CC={cc} CXX={cxx} cmake --build . -- -j16')

build_gdal()

