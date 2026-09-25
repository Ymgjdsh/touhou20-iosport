import pathlib,subprocess,os
root=pathlib.Path(__file__).resolve().parents[2]
os.environ.setdefault('DEVELOPER_DIR','/Applications/Xcode.app/Contents/Developer')
includes=[root/'ios/compat',root/'ios/src',root/'native_recovered',root/'include',root/'source_reconstruction']+[p for p in (root/'source_reconstruction').iterdir() if p.is_dir()]
cmd=['xcrun','clang++','-std=c++20','-DTH20_IOS=1','-include',str(root/'ios/compat/port_prefix.hpp'),'-Wno-invalid offsetof'.replace(' ','-'),'-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-fast-math','-ffp-contract=off','-fno-strict-aliasing','-Wl,-dead_strip']
for p in includes:cmd+=['-I',str(p)]
for source in ['ios/tests/combat_native_test.cpp','source_reconstruction/bullet_system/metadata.cpp','source_reconstruction/bullet_system/state.cpp','source_reconstruction/core_scheduler/scheduler.cpp','source_reconstruction/damage_regions/regions.cpp','source_reconstruction/ecl_vm/math.cpp','source_reconstruction/laser_system/base.cpp','source_reconstruction/laser_system/type2_path.cpp','source_reconstruction/runtime_core/runtime_core.cpp']:
 cmd += [str(root/source)]
cmd += ['-o',str(root/'combat-native-test')]
subprocess.run(cmd,check=True)
subprocess.run([str(root/'combat-native-test')],check=True)
