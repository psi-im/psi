#!/bin/sh

ref_commit=000473ebfd463c910ee81d599a7982d8753b83dc # change version file to 1.1

cd $(dirname "$0")
git rev-list --count ${ref_commit}..HEAD
