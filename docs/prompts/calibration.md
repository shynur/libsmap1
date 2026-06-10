@examples/rbk34/calibration-rbk34.cp 和 @examples/rbk35/calibration-rbk35.cp 是我新加的标定文件 (robot.cp) 样例, 取自真实日志包:
- rbk34 来自 robokit 日志包的 `models/robot.cp` (备份在 `models/bak/cp/*.robot.cp`).
- rbk35 来自日志包的 `private/calibrations/robot.cp`.

标定文件记录每个设备 (laser / motor / camera / IMU / ...) 标定后的安装参数修正值与标定通过状态.
它是 read-only 的, 我们不会去编辑它.

两个版本都是 JSON, 但结构不同:
- rbk34: 顶层 `{"deviceTypes": [...]}`, 参数树与 robot model 文件一致 (arrayParam / comboParam / 标量叶子).
- rbk35: 顶层 `{"model": {...}}`, 扁平键值表, 键为 `<DeviceType>.<DeviceName>.<param.path>` 点分隔路径;
  形如 `<...>.<XxxCalib>: "Passed"` 的项是标定状态而非参数.

---

因此, libsmap1 需要新增这些功能:
- 表示标定数据的 IR.  由于我们不需要编辑标定文件, 所以只需要存储归一化后的数据:
  设备 -> (参数键值 + 标定状态) 的列表.
- 从文件中读入标定数据, 转换成 protobuf 对象.  标定文件同样区分不同的版本.
