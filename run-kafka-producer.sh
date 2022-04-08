#!/bin/bash

./build/src/kafka-producer/kafka-producer host.docker.internal:29092 topic$1 $1