中间格式 (IR) 采用 Protocol Buffers 定义, 且能与 JSON 进行序列化/反序列化.

任何版本的原始地图格式都必须可以无损地转换为 IR;
IR 在转换到原始格式时, 需要保留尽可能多的信息.

IR 需要较好的可扩展性 (或者从一开始就把可能需要存储的各类信息都设计好), 以便未来适配新的地图格式.

未来还需要允许编辑 IR 格式的地图 (这是其它项目的任务), 而对地图的修改方式有很多种 (e.g., 添加各类站点, 在站点之间添加 5 阶 Bézier curve whose 中间的四个控制点两两重合), 需要设计良好的 IR 以便于修改.

---

目前已经在 @examples/ 目录下放置了两种版本的原始地图文件:
- RBK v3.4: @examples/rbk34/raw-json.smap 单文件.  文档在 @examples/rbk34/doc/ 下 (仅供参考, 因为它们可能是过时的, 请以 @examples/rbk34/proto/message_map.proto 为准).
- RBK v3.5: @examples/rbk35/raw-folder/ 整个文件夹作为一个地图.  文档在 @examples/rbk35/doc/ 下 (仅供参考, 因为它们可能是过时的, 请以 @examples/rbk35/proto/message_map.proto 为准).

- 对于 RBK v3.4 的地图格式: `*.smap` 本质上就是 JSON.
- 对于 RBK v3.5 的地图格式: `*.2dft` `*.2dlh` `*.2dpc` 是二进制文件, @examples/rbk35/file-header-parser/ 用于读取二进制文件的 header, 后续是完整的 proto 二进制对象; `*.smap` 就是 JSON 文件.

---

请你设计 IR.

(为了检验 IR 的兼容性, 你需要写些简短的程序, 通过 protobuf 或 JSON 解析器去实际查看示例地图文件的内部字段.)
