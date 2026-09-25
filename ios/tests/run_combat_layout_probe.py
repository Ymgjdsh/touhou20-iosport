import pathlib,subprocess,os
root=pathlib.Path(__file__).resolve().parents[2]
os.environ.setdefault('DEVELOPER_DIR','/Applications/Xcode.app/Contents/Developer')
includes=[root/'ios/compat',root/'ios/src',root/'native_recovered',root/'include',root/'source_reconstruction']+[p for p in (root/'source_reconstruction').iterdir() if p.is_dir()]
cmd=['xcrun','clang++','-std=c++20','-DTH20_IOS=1','-include',str(root/'ios/compat/port_prefix.hpp'),'-Wno-invalid offsetof'.replace(' ','-')]
for p in includes:cmd+=['-I',str(p)]
cmd += [str(root/'ios/tests/combat_layout_probe.cpp'),'-o',str(root/'combat-layout')]
subprocess.run(cmd,check=True)
subprocess.run([str(root/'combat-layout')],check=True)
