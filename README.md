# Agile Modbus MCU Demos

## 1、介绍

1. 该仓库为轻量级协议栈 [Agile Modbus](https://github.com/loogg/agile_modbus) 在 MCU 上的例子。
2. 提供 裸机、RT-Thread 通过串口实现 `Modbus` 从机，通过 [Agile Modbus](https://github.com/loogg/agile_modbus) 的 `p2p_master` 和 `broadcast_master` 特殊功能码示例实现点对点和快速数据流广播升级固件
3. 基于正点原子探索者开发板

目录结构

| 名称 | 说明 |
| ---- | ---- |
| bootloader | bootloader 引导程序 |
| NOS | 裸机 Demo |
| RTT | RT-Thread Demo |

## 2、使用
