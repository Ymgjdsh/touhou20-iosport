# TH20 iOS Port

一个面向 iOS 14 及以上设备的《东方锦上京》原生移植工程。项目使用恢复后的 C++ 游戏逻辑和 UIKit/OpenGL ES 平台适配层，不依赖浏览器运行时。

## 项目内容

- `source_reconstruction/`：恢复的游戏逻辑模块。
- `native_recovered/`：经过校验的原生恢复代码。
- `src/`、`include/`：资源、脚本和运行时解析代码。
- `ios/`：iOS 主机、输入、渲染、音频、设置界面和构建配置。
- `tests/`：源码和平台行为验证。
- `tools/`：构建、资源校验和打包辅助工具。

仓库不包含原版游戏数据、原版可执行文件或 IPA。运行和打包需要用户合法拥有的游戏资源，并应通过本地路径传给构建脚本。

## iOS 构建

环境要求：macOS、Xcode 14 或更新版本、CMake，以及 iOS 14 SDK。将 `th20.dat` 和 `thbgm.dat` 放在工程外部的资源目录中，然后运行：

```sh
TH20_CONFIGURATION=Release \
TH20_SDK=iphoneos \
TH20_TARGET=th20_ios_game \
TH20_ASSET_DIR=/path/to/owned/game-assets \
bash ios/build_ios.sh
```

设备构建输出位于 `build-native-iphoneos/Release-iphoneos/th20_ios_game.app`。可使用 `ios/tools/package_ipa.py` 对设备包进行架构、资源、签名和 ZIP 完整性检查后再安装到测试设备。工程默认生成 ARM64、iOS 14+ 的未签名包，实际安装需要设备端允许的签名方式（例如 TrollStore 或开发签名）。

## 触摸操作

设置中支持 `Hybrid`、`Drag` 和 `Joystick` 三种移动方式，也支持隐藏虚拟按键、调整按键与摇杆位置、左右手布局、帧率和渲染清晰度。

- `Z`：射击 / 确认
- `S`：低速移动
- `X`：符卡 / 返回
- 战斗中双指轻按：释放符卡
- 战斗中三指长按：暂停
- 菜单中双指轻按：返回

## 资源与版权

本仓库只发布移植代码和构建工具。游戏数据、角色图像、音乐以及原作内容的版权归其各自权利人所有。请仅使用自己合法取得的资源，并遵守当地法律及原作许可要求。

## 致谢

项目的 iOS 交互和构建组织参考了 [th07-ios-port](https://github.com/Ymgjdsh/th07-ios-port) 的公开实现思路。感谢 Team Shanghai Alice 的原作，以及相关逆向和移植社区提供的研究资料。
