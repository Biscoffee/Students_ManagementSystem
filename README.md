# Students_ManagementSystem  

## 感谢3g倪神学长的测试🫡

## 4.18日更新：
### 修复：
修复了管理员端无法添加学生成绩的bug
### 优化：
优化了管理员端导出成绩到文件的逻辑  

优化了输入模式（取消两次回车） 

优化了班级格式判断逻辑  

优化了邮箱验证逻辑  


### 新增：
新增清屏功能  

新增了教师和管理员的注册验证功能（需使用指定密钥） 



# 学生成绩管理系统三端功能说明


## 👨🎓 学生端功能

1. 个人成绩查询
   - 实时查看各科成绩
   - 显示班级排名/平均分对比
2. 成绩申诉
   - 填写申诉理由（200字以内）
   - 查看申诉处理进度
3. 班级成绩总览
   - 查看本班成绩分布直方图
   - 导出个人成绩单（TXT格式）



## 👩🏫 教师端功能
1. 班级管理
   - 添加/修改本班学生成绩
   - 批量导入成绩（CSV格式）
2. 数据分析
   - 生成班级成绩统计报表
   - 自动计算：平均分/最高分/及格率
3. 申诉处理
   - 审核本班学生申诉
   - 修改成绩需填写审批意见
4. 班级管理
   - 设置考试科目权重
   - 导出班级成绩存档


## 👨💻 管理员端功能
1. 账户管理
   - 教师/学生账号增删改查
   - 重置任意账户密码
2. 全局控制
   - 跨班级成绩调整
   - 系统参数配置（如分数阈值）
3. 数据维护
   - 数据库备份/恢复
   - 清理异常数据
4. 权限管理
   - 分配教师管辖班级
   - 查看操作日志
5. 系统监控
   - 登录异常报警
   - 存储空间监控
