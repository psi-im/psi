#!/usr/bin/env python3

# emoji.py - Emojis registry generator
# Copyright (C) 2020  Sergey Ilinykh
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.


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
            if "skin tone" in line:
                continue  # will be implemented later
            match = re.match(r'^(.+); fully-qualified.*# .* E\d+\.\d+ (.*)', line)
            if not match:
                continue
            desc = match.group(2).split(':')
            code = "".join([chr(int(c, 16)) for c in match.group(1).strip().split()])
            subgroup["emojis"].append((code, desc[0].strip()))


def cleanup_empty():
    global data
    for group in data:
        group["subgroups"] = [s for s in group["subgroups"] if len(s["emojis"])]
    data = [g for g in data if len(g["subgroups"])]


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
    cleanup_empty()
    generate_ranges()
    generate_cpp_db()
