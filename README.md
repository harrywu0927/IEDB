# IEDB_STD_Functions

## 介绍
装备数据库标准函数

## Dependency
### For Linux(Ubuntu18.04 or later):
```sudo apt install python3.8-dev```

#### 为python3.8安装numpy、pandas、sklearn、matplotlib:
若不使用Anaconda：

```sudo apt install python3-pip liblapack-dev gfortran```

```python3.8 -m pip install pybind11 pythran Cython numpy pandas scipy sklearn matplotlib```

- 若安装过程中出现另外的依赖缺失，安装即可

- 若使用Intel芯片，可选装sklearn加速包：

```python3.8 -m pip install scikit-learn-intelex```

### For macOS:
```brew install python@3.9```
## 使用说明

1.  在文件根目录中，```cmake .```
2.  ```make```

## 参与贡献

1.  Fork 本仓库
2.  新建 Feat_xxx 分支
3.  提交代码
4.  新建 Pull Request
