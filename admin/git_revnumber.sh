#!/bin/sh

ref_commit=4b8a3473ee1de59f84627000cba462e05f8a9b84 # change version file to 1.2

cd $(dirname "$0")
git rev-list --count ${ref_commit}..HEAD
