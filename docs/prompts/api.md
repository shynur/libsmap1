与我讨论并设计 libsmap1 的 public API.

IR 的原始需求在 @docs/prompts/design-IR.md , 由此设计出了 @proto/smap1/ir.proto (它的文档在 @docs/ir.md ).
(若有必要, 我们可以修改 IR 的 设计 / 文档 / 实现, 目标是设计合适的 API.)

路线:
1. 讨论 libsmap1 需要提供哪些功能; then 记录到文档
2. 依次设计具体的 C++ API (class / function / variable / etc.) 并写到 @include/smap1/ 下合适的头文件中.  (公开 API 使用 Doxygen 文档, 别单独记录到文档.)  (对于 public API 的设计, 既要便于调用方使用, 也要便于维护者实现.)
3. 实现 API
