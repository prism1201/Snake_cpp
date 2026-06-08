# 贪吃蛇大作战

基于 **Qt 6 + CMake + C++17** 的图形化贪吃蛇游戏。

## 运行方式

用 Qt Creator 打开 `Snake_cpp/CMakeLists.txt`，选择 MinGW 64-bit 套件构建运行。

也可以在构建目录中直接运行编译好的 `Snake_cpp.exe`（需确保同目录下有 Qt 运行库及素材文件）。

## 玩法说明

| 操作 | 按键 |
|------|------|
| 移动 | 方向键 / WASD |
| 加速 | 按住 Shift |
| 暂停/继续 | 空格键 |
| 返回菜单 | R 键 |
| 静音切换 | M 键 |
| 选择难度 | 1 / 2 / 3（菜单界面） |
| 开始游戏 | 回车键 / 点击按钮（菜单界面） |

## 食物类型

| 食物 | 效果 |
|------|------|
| 苹果 | +10 分 |
| 金星 | +50 分 |
| 爱心 | 获得一次复活机会（最多 3 次） |
| 法球 | 临时清除一半障碍物，持续 15 秒 |
| 冰莓 | 蛇进入冰冻减速状态，持续 10 秒 |
| 彩钻 | 进入炫彩无敌状态 8 秒，可撞碎障碍物（被撞碎的障碍物 15 秒后重生） |

## 难度设置

| 难度 | 初始速度 | 速度上限 | 障碍物数量 |
|------|---------|---------|-----------|
| 简单 | 145ms/步 | 90ms/步 | 10 个 |
| 普通 | 120ms/步 | 68ms/步 | 18 个 |
| 困难 | 95ms/步 | 50ms/步 | 28 个 |

## 项目结构

```
Snake_cpp/
├── CMakeLists.txt          # CMake 构建配置
├── include/                # 头文件
│   ├── GameWidget.h        # 主窗口（绘图、输入、音效、主流程）
│   ├── Snake.h             # 蛇身体与移动逻辑
│   ├── Food.h              # 食物位置生成
│   ├── ScoreManager.h      # 分数与最高分管理
│   └── GameController.h    # 游戏状态机
├── src/                    # 源文件
│   ├── main.cpp            # 程序入口
│   ├── GameWidget.cpp
│   ├── Snake.cpp
│   ├── Food.cpp
│   ├── ScoreManager.cpp
│   └── GameController.cpp
└── resources/              # Qt 资源文件
    ├── gamewidget.ui
    └── resources.qrc
```

## 构建依赖

- Qt 6.5+ (Core / Widgets / Multimedia / LinguistTools)
- CMake 3.19+
- C++17 编译器
