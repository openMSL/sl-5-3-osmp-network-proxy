# SL-5-3 OSMP Network Proxy

![Credibility Assessment](../../actions/workflows/credibility_assessment.yml/badge.svg)

This Network Proxy FMU can receive SensorView and SensorData via TCP/IP using ZeroMQ.
The received data is directly given to the FMU output.
The proxy can also send SensorView oder SensorData received as FMU input via TCP/IP to a given IP address and port.

## Parameterization

The following FMI parameters can be set.
Either `sender` or `receiver` have to be set to _true_.
Otherwise, the FMU will return with an error.

| Type    | Parameter        | Default     | Description                                                                                                                                                                                                                                                             |
|---------|------------------|-------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Boolean | `sender`         | _true_      | Set if Proxy shall send data via TCP/IP                                                                                                                                                                                                                                 |
| Boolean | `receiver`       | _false_     | Set if Proxy shall receive data via TCP/IP                                                                                                                                                                                                                              |
| Boolean | `pushpull`       | _false_     | If true, use [Push/Pull](https://learning-0mq-with-pyzmq.readthedocs.io/en/latest/pyzmq/patterns/pushpull.html) ZeroMQ configuration, if false, use [Client/Server](https://learning-0mq-with-pyzmq.readthedocs.io/en/latest/pyzmq/patterns/client_server.html) config. |
| String  | `ip`             | _127.0.0.1_ | IP address of TCP connection                                                                                                                                                                                                                                            |
| String  | `port`           | _3456_      | Port of TCP connection                                                                                                                                                                                                                                                  |
| Integer | `wait_time_in_s` | _5_         | In Client/Server configuration: wait time to receive message after request. When the network proxy does not receive a message in this wait time, it will throw an error. If this is set to _0_, the network proxy will wait indefinitely.                               |

## Interface

The Proxy can receive SensorView or SensorData as input.

## Build Instructions

### Build Model in Ubuntu 20.04 / 22.04

1. Clone this repository **with submodules**:

    ```bash
    git clone https://github.com/openMSL/sl-5-3-osmp-network-proxy.git --recurse-submodules
    ```

2. Install dependencies:

   ```bash
   sudo apt install libzmq3-dev
   ```

3. Build the model by executing in the extracted project root directory:

    ```bash
    mkdir cmake-build
    cd cmake-build
    # If FMU_INSTALL_DIR is not set, CMAKE_BINARY_DIR is used
    cmake -DCMAKE_BUILD_TYPE=Release -DFMU_INSTALL_DIR:PATH=/tmp ..
    make
    ```

4. Take FMU from `FMU_INSTALL_DIR`
