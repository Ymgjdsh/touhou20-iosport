import pathlib,subprocess,os
root=pathlib.Path(__file__).resolve().parents[2]
os.environ.setdefault('DEVELOPER_DIR','/Applications/Xcode.app/Contents/Developer')
includes=[root/'ios/compat',root/'ios/src',root/'native_recovered',root/'include',root/'source_reconstruction']+[p for p in (root/'source_reconstruction').iterdir() if p.is_dir()]
compiler=subprocess.check_output(['xcrun','--find','clang++'],text=True).strip()
base=[compiler,'-std=c++20','-DTH20_IOS=1','-include',str(root/'ios/compat/port_prefix.hpp'),'-Wno-invalid offsetof'.replace(' ','-'),'-fno-fast-math','-ffp-contract=off','-fno-strict-aliasing']
for p in includes:base+=['-I',str(p)]
source=str(root/'ios/tests/enemy_laser_parameters_test.cpp')
program=str(root/'enemy-laser-parameters-test')
subprocess.run(base+['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer',source,'-o',program],check=True)
subprocess.run(['uname','-m'],check=True)
subprocess.run(['file',program],check=True)
subprocess.run([program],check=True)
sdk=subprocess.check_output(['xcrun','--sdk','iphoneos','--show-sdk-path'],text=True).strip()
# Real ARM64 compile, separate from execution on the Intel macOS host.
subprocess.run(base+['-target','arm64-apple-ios14.0','-isysroot',sdk,'-fsyntax-only',source,str(root/'source_reconstruction/gameplay/enemy_opcode_laser.cpp')],check=True)
print('PASS arm64-apple-ios14.0 syntax and layout checks for parameter tests and production opcode handler')
