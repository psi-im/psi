#!/usr/bin/env python3
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.

import re
import pprint

data = []
ranges = []

template_main="""// This is a generated file. See emoji.py for details
// clang-format off
static std::vector<EmojiRegistry::Group> db = {{{groups}
}};

static std::map<quint32, quint32> ranges = {{
    {ranges}
}};

// clang-format on
"""

template_group="""
    {{
        QT_TR_NOOP("{name}"),
        {{{subgroups}
        }}
    }}"""

template_subgroup = """
            {{
                "{name}",
                {{{emojis}
                }}
            }}"""

template_emoji = """
                    {{
                        "{code}",
                        "{desc}"
                    }}"""

def reset():
    global group, subgroup
    group = dict(name="", subgroups=[])
    subgroup = dict(name="", emojis=[])


def parse(f):
    for line in f:
        if line.startswith("# group:"):
            reset()
            group["name"] = line.split(":")[1].strip()
            data.append(group)
        elif line.startswith("# subgroup:"):
            subgroup = dict(name=line.split(":")[1].strip(), emojis=[])
            group["subgroups"].append(subgroup)
        elif not line.strip() or line.startswith("#"):
            continue
        else:
            match = re.match(r'^(.+);.*# .* E\d+\.\d+ (.*)', line)
            desc = match.group(2).split(':')
            code = "".join([chr(int(c, 16)) for c in match.group(1).strip().split()])
            subgroup["emojis"].append((code, desc[0].strip()))


def generate_ranges():
    emojis=[]
    for g in data:
        for sg in g["subgroups"]:
            emojis += sg["emojis"]
    emojis = sorted(emojis)
    cur_code = range_start = 0
    for emoji in emojis:
        new_code = ord(emoji[0][0])
        if new_code != cur_code and new_code != cur_code + 1:
            if range_start != 0:
                ranges.append((range_start, cur_code))
            range_start = new_code
        cur_code = new_code
    ranges.append((range_start, cur_code))


def generate_cpp_db():
    print(template_main.format(groups=",".join([
        template_group.format(name=group["name"], subgroups=",".join([
            template_subgroup.format(name=sub["name"], emojis=",".join([
                template_emoji.format(code=emoji[0], desc=emoji[1])
                for emoji in sub["emojis"]
            ]))
            for sub in group["subgroups"]
        ]))
        for group in data
    ]), ranges=",\n    ".join([f"{{{r[0]}, {r[1]}}}" for r in ranges])))


# https://unicode.org/Public/emoji/13.0/emoji-test.txt
with open("emoji-test.txt") as f:
    reset()
    parse(f)
    generate_ranges()
    generate_cpp_db()
