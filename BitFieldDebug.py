# Written by Caleb Yates 8-16-23


import re

# LLDB Python API still doesn't seem to have a method to get the actual value of template arguments. It can retrieve the
# names,types, and number of template args, but not the value. Luckily, we can cheat and get these values from the
# BitField type name declared. This allows us to perform the calculations to get the actual value of the specific BitField.

def BitFieldDebug(valobj, internal_dict):
    name = valobj.GetTypeName()
    nums = re.findall('\d+', name)
    position = int(nums[0])
    size = int(nums[1])
    value = valobj.GetChildMemberWithName("storage").GetValueAsUnsigned(0)
    val = (value >> position) & ((1 << size) - 1)
    return 'Value: ' + str(hex(val))
