@examples/rbk34/robot_model-rbk34.json 和 @examples/rbk35/robot_model-rbk35.json 是我新加的 robot 的 model 文件.
Robot model 是 read-only 的, 我们不会去编辑它.

它可以辅助我们在编辑器上绘制站点: 知道了站点的坐标和朝向, 还需要站点轮廓, 这个轮廓就由机器人的尺寸决定.

因此, libsmap1 中表示地图的原生 C++ class (也就是 IR 的抽象层) 应当提供一个方法, 用来设置要放置在该 地图中的 robot 的相关信息.
举例: 如果地图知道 robot 的尺寸, 就可以拒绝某些不合理的站点位置.

---

因此, libsmap1 需要新增这些功能:
- 表示 robot model 的 IR.  由于我们不需要编辑 robot model, 所以只需要存储我们关心的数据.  数据结构也按照我们自己的喜好来定义.
- 从文件中读入 robot model, 转换成 protobuf 对象.  模型文件同样区分不同的版本.
- 地图 class 提供与 robot model 相关的 API.
