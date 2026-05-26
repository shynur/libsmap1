# libsmap1

该项目是一个地图格式中间层 IR 库, 提供多种编辑操作, 且可用来兼容多种版本的原始地图格式文件.

旨在将所有编辑操作都放在 IR 层, 原始格式被转换到 IR 再被编辑再被转换回原始格式.

## Install

```bash
cd /path/to/smap1/
conan create . --build=missing
```

## 功能

### codec

输入的原始地图格式可能有多种形式: 单文件, 文件夹, etc.
