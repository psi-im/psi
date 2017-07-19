#!/bin/sh

ref_commit=2812a0af876f47b9001fcd3a4af9ad89e2ccb1ea # change version file to 1.0

cd $(dirname "$0")
git rev-list --count ${ref_commit}..HEAD
