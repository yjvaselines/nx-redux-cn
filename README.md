# nx-redux-cn

NXRedux（NextUI 旧版分支）**中文增强版**，基于上游 [mohammadsyuhada/nx-redux](https://github.com/mohammadsyuhada/nx-redux) **v2.1.2**，面向 Trimui Brick pro / tg5040 掌机。

本分支在保留 v2.1.2 全部上游架构与功能的基础上，完成系统大部分中文化。少量工具和全部模拟内核设置未汉化。

***

## 功能变化一览

### 一、新功能

| 功能           | 说明                                                                                                                                                         |
| ------------ | ---------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 多图片格式支持      | 游戏封面、媒体查看器等全场景支持 **.webp / .gif**（gif 取第一帧），查找优先级 webp > jpg/jpeg > png（jpg/jpeg 与 png 为上游官方已支持）。解码依赖**改版 libSDL2\_image**（见下方 Release 说明），原版库不支持 webp/gif |
| pico8 游戏封面   | `.p8` / `.p8.png` 格式游戏自身即封面，无需再放 .media 封面文件；纯文本 .p8 经前 8 字节 PNG 签名校验后自动跳过                                                                                 |
| 拼音搜索         | 游戏搜索支持拼音首字母匹配（如输入 `xj` 可命中名称中带"仙剑"的Rom，对于街机游戏，因为Rom不可改名，搜索功能同样可以索引map.txt别名）                                                                               |
| 随机游戏         | 搜索页新增启动一个随机游戏功能                                                                                                                                            |
| START 全局搜索   | 主界面、游戏列表、收藏夹等列表场景均可按 START 打开搜索（游戏中和应用内除外）                                                                                                                 |
| 封面显示加大       | game lists layout 的 list art = mix 时，封面区域宽度扩大到屏幕宽度 50%，游戏名与封面重叠时保持收缩滚动                                                                                     |
| 搜索结果显示封面     | 搜索结果列表与游戏列表 mix 模式展示规则一致，显示对应游戏封面                                                                                                                          |
| 中文电视台/电台     | 内置CCTV全套+93个中文电台——流媒体内容来自第三方，不保证长期可用性                                                                                                                      |
| PS 汉化游戏 BIOS | PS 游戏支持汉化专用 BIOS：将汉化游戏专用 BIOS 放入 `/Bios/PS/` 下并改名为与游戏 Rom 同名（如 武藏传.bin、寂静岭.bin、格兰蒂亚.bin 等）                                                                 |

### 二、Bug 修复

| 问题                | 修复                                                                   |
| ----------------- | -------------------------------------------------------------------- |
| 搜索无结果时残留封面        | 搜索关键词无结果时，不再错误显示该列表内其他游戏的封面                                          |
| F1/F2 自定义键始终显示"关" | 已绑定工具后设置页仍显示"关"的问题修复，现正确显示所绑定的工具名                                    |
| 音乐播放器电台上限         | 电台数量上限由 32 条升为 2048 条，其帮助页同步更新                                       |
| 中文台名乱码            | 在线电视播放 OSD 为 ASCII 点阵字体，中文台名出现乱码；播放 OSD 现剥离非 ASCII 字符显示（加载画面仍显示完整中文） |
| 多行说明文字截断          | 选项菜单多行描述文字原硬性截断 2 行，改为按实际空间自适应（最多 4 行）                               |

### 三、全面中文化

- **系统设置**：全部页面中文，包括 v2.1.2 新增的"模拟器快捷键"（Emulator Shortcuts）页、F1/F2 自定义键、布局（Layouts）、外观、系统等

- **游戏内菜单**（minarch）：核心选项 / 着色器 / 作弊码 / 按键设置 / 成就 / 保存更改，及全部提示条（读档、保存、退出等）

- **设备同步**（Device Sync）：数据线 / Wi-Fi 双模式全部流程文案

- **图片查看器**（Image Viewer）：加载、保存、旋转提示等

- **游戏切换器**：标题"最近"、列表布局提示条、"继续 / START / 移除"等

- **媒体播放器**（Media Player）：启动画面、主菜单（影片库 / 在线电视）、面包屑、按键提示、右键菜单（退出应用 / 浏览频道 / 移除频道）、加载与错误提示

- **音乐播放器**（Music Player）：界面、各加载显示名、菜单项、面包屑、按键提示、菜单弹出项、手动设置帮助页、后台播放唤醒提示等全面汉化

- **按键帮助组件**："按任意键关闭"等公共提示

> 按键名（A/B/X/Y/START 等）保留英文不翻译。

### 四、保留的上游 2.1.2 特性

- 模拟器快捷键系统重构（连发设置移入独立页）

- D-pad/摇杆切换快捷键（"切换方向键摇杆"）

- 游戏切换器新增 List 布局与按键提示开关

- 全部存档、成就（RetroAchievements）、同步等原有功能

***

## 编译

```bash
# 使用 tg5040 交叉工具链容器（ghcr.io/loveretro/tg5040-toolchain）
docker run --rm -v $(pwd)/workspace:/root/workspace -w /root/workspace \
  ghcr.io/loveretro/tg5040-toolchain:latest make PLATFORM=tg5040
```

- `skeleton/`：真机固件目录骨架（未改动的基础文件，刷入参考）

- `workspace/all/`：各组件源码；`workspace/tg5040/`：平台组件（libmsettings 等）

## Release

Releases 页提供**完整刷入包**（NXRedux-2.1.2-cn-brickpro.zip，适配 Trimui Brick Pro / tg5040）：包含全部编译产物（.elf / .so，按真机 `.system` 目录结构组织）以及未改动的基础文件，解压到 SD 卡根目录覆盖即可。

刷入包已**预置**以下配置，解压后自动就位，无需手动操作：

- 在线电视频道：`.userdata/shared/media-player/tv/channels.json`

- 电台收藏：`.userdata/shared/music-player/radio/stations.txt`

### 改版 libSDL2\_image（必装）

webp/gif 封面解码依赖**改版** **`libSDL2_image-2.0.so.0`**（上游官方库已支持 jpg/jpeg/png，改版库额外增加 webp 与 gif（第一帧）），作为单独附件随 Release 提供。它不属于 SD 卡刷入包范围，需要手动放置：

- 真机路径：`/usr/trimui/lib/libSDL2_image-2.0.so.0`（固件分区，**不在 SD 卡内**，刷 SD 卡包不会覆盖它）

- 摆放方法：将 Release 附件 `libSDL2_image-2.0.so.0` 复制到 SD 卡任意临时位置，再通过 Telnet/adb/系统自带files.pak文件管理器 等方式移动到上述固件路径（覆盖前建议备份原文件）：

  ```
  mv /mnt/SDCARD/libSDL2_image-2.0.so.0  ——>  /usr/trimui/lib/libSDL2_image-2.0.so.0
  ```

- 若不放置该库，webp/gif 封面将无法显示（jpg/jpeg/png 不受影响）

## 许可

同上游（见 [LICENSE](LICENSE)）。
