# R005/r1 验证摘要

- 被验证源码：`bd7d6de5399b226ef3b667d54b14304c9c82655a`。
- Opus执行：`python -B -m unittest -v protocol.test_contract`，9/9通过；`python -B f730_bridge/tools/cdc_echo_test.py --self-test`，退出0。
- 固件：隔离CMake configure退出1，检查路径下缺少兼容ARM工具链，后续固件build未执行。该结论不表示所有机器都缺工具链。
- 未验证：当前固件镜像、USB实机、FMC和完整链路。不提供新HEX或板测通过结论。

Astra核验交付清单及基线。完整原始日志保留在本地工作区 `artifacts/collaboration/R005/r1/opus/logs/f730_*`，不是本仓附件。其他机器按HANDOFF指定工具链后重新构建。
