

## v1.3.0 (20230713)

### en
- Update the library of libraft for more stability and higher performance
- After the service is started, use the deployed node
- Regarding the issue of modifying the data directory, / has been added. Please be careful

 
### cn

- 更新libraft,更稳定,性能更高
- 服务启动以后, 以部署的节点为准
- 修改数据目录的问题, 增加了 /, 请务必注意

## v1.2.1 (20220823)

### en

- use -static-gcc -static-libstdc++
### cn

- 默认使用-static-gcc -static-libstdc++, 静态编译, 以适配不同GCC的OS

## 20220805 v1.2.0

### cn
- 增加唯一字符串的生成
- 支持唯一字符串超时设置
- 支持查询是否存在字符串

### en
- feature: add unique random string
- feature: support random string expire time
- feature: support query random string exists

## 20220725 v1.1.0

### cn
- 增加循环计数接口

### en
- add circleCount

## 20220425 v1.0.0

### cn
- 第一个版本, raft参数可以配置
- 可以指定起始计数值
- 支持web命令, 可以查询和设置技术

### en
- In the first version, the raft parameter can be configured
- You can specify the starting count value
- It supports web commands and can query and set technology


