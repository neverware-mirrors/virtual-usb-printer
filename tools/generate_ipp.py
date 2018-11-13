#!/usr/bin/env python

"""
This script is used to convert an encoded IPP message into a human-readable JSON
form.

To run the script:
  generate_ipp <input_filename> <output_filename>

  This will read in the contents from |input_filename|, convert the contents
  into a human readable JSON form, and write the results to |output_filename|.

The input file should be in the form:
  2-character hex strings separated by whitespace.

  Ex:
    01 47 00 12 61 74 74 72 69 62 75 74 65 73 2d 63 68 61 72 73 65 74 ...
"""
import json
import re
import sys

# Returns whether |ascii_val| is one of the IPP delimiter tags. This is used to
# determine when a new attribute group is starting, or if we have reached the
# end of the attributes.
def is_ipp_delimiter(ascii_val):
  delimiters = {0x01, 0x02, 0x03, 0x04}
  return ascii_val in delimiters


# Converts the |ipp_delimiter| into a human readable form. If |ipp_delimiter| is
# not a supported IPP delimiter then raises SystemExit.
def get_delimiter_name(ipp_delimiter):
  delimiters_map = {
      0x01: 'operationAttributes',
      0x02: 'jobAttributes',
      0x03: 'endOfAttributes',
      0x04: 'printerAttributes'
  }
  if ipp_delimiter in delimiters_map:
    return delimiters_map[ipp_delimiter]
  print("{} is not a supported ipp delimiter".format(ipp_delimiter))
  raise SystemExit


# Converts the |value_tag| into a human readable form. If |value_tag| is not
# a supported IPP value tag then raises SystemExit.
def get_value_type(value_tag):
  values_map = {
      0x13: 'no-value',
      0x21: 'integer',
      0x22: 'boolean',
      0x23: 'enum',
      0x30: 'octetString',
      0x31: 'dateTime',
      0x32: 'resolution',
      0x33: 'rangeOfInteger',
      0x34: 'begCollection',
      0x37: 'endCollection',
      0x41: 'textWithoutLanguage',
      0x42: 'nameWithoutLanguage',
      0x44: 'keyword',
      0x45: 'uri',
      0x46: 'uriScheme',
      0x47: 'charset',
      0x48: 'naturalLanguage',
      0x49: 'mimeMediaType',
      0x4a: 'memberAttrName'
  }
  if value_tag not in values_map:
    print("{} is not a supported IPP value tag".format(value_tag))
    raise SystemExit
  return values_map[value_tag]


# Helper function that ensures that extracting a value of length |length|
# starting from |start|, does not run off the end of |data|.
def check_data_boundary(data, start, length):
  if len(data) - start < length:
    print("Length is greater than remaining elements in data")
    raise SystemExit


# Extracts a string value of |length| bytes from |data|, starting from |start|.
# Returns a tuple containing the string extracted, and the index to continue
# parsing from.
def extract_string_value(data, start, length):
  check_data_boundary(data, start, length)
  value = ''
  for i in range(start, start + length):
    value += chr(data[i])
  return (value, start + length)


# Extracts a byte value from |data[start]|. A byte value is expected to be
# exactly 1 byte. Returns a tuple containing the byte extracted, and the index
# to continue parsing from.
def extract_byte_value(data, start, length):
  if length != 1:
    print("extract_byte_value: Length must be exactly 1")
    raise SystemExit
  check_data_boundary(data, start, length)
  return (data[start], start + length)


# Extracts a boolean value from |data[start]|. A boolean value is expected to be
# exactly 1 byte. Returns a tuple containing the value extracted, and the index
# to continue parsing from.
def extract_boolean_value(data, start, length):
  if length != 1:
    print("extract_boolean_value: Length must be exactly 1")
    raise SystemExit
  check_data_boundary(data, start, length)
  value = data[start]
  return (value != 0, start + length)


# Extracts an integer value from |data[start]|. An integer value is expected to
# be exactly 4 bytes. Returns a tuple containing the value extracted, and the
# index to continue parsing from.
def extract_integer_value(data, start, length):
  if length != 4:
    print("extract_integer_value: Length must be exactly 4")
    raise SystemExit
  check_data_boundary(data, start, length)
  value = 0
  for i in range(start, start + length):
    ascii_val = data[i]
    value = (value * 256) + ascii_val
  return (value, start + length)


# Extracts an array of bytes of length |length| from |data|, starting from
# |start|. Returns a tuple containing the array extracted, and the index to
# continue parsing from.
def extract_bytes_value(data, start, length):
  check_data_boundary(data, start, length)
  value = []
  for i in range(start, start + length):
    value.append(data[i])
  return (value, start + length)


# Extracts a resolution from |data| starting from |start|. A resolution is
# expected to be exactly 9 bytes (2 integers, 1 byte). Returns a tuple
# containing the resolution extracted, and the index to continue parsing from.
def extract_resolution_value(data, start, length):
  if length != 9:
    print("extract_resolution_value: Length must be exactly 9")
    raise SystemExit
  check_data_boundary(data, start, length)
  (x_res, new_start) = extract_integer_value(data, start, 4)
  (y_res, new_start) = extract_integer_value(data, new_start, 4)
  (units, new_start) = extract_byte_value(data, new_start, 1)
  return ([x_res, y_res, units], new_start)


# Extracts a range from |data| starting from |start|. A range is expected to be
# exactly 8 bytes (2 integers). Returns a tuple containing the resolution
# extracted, and the index to continue parsing from.
def extract_range_value(data, start, length):
  if length != 8:
    print("extract_range_value: Length must be exactly 8")
    raise SystemExit
  check_data_boundary(data, start, length)
  (lower, new_start) = extract_integer_value(data, start, 4)
  (upper, new_start) = extract_integer_value(data, new_start, 4)
  return ([lower, upper], new_start)


# Uses |value_type| to determing the correct value extraction function to call.
# It then calls the appropriate extraction function passing |data|, |start|, and
# |length|. Returns a tuple containing the value extracted and the index to
# continue parsing from.
def extract_value(data, value_type, start, length):
  check_data_boundary(data, start, length)
  function_map = {
      'no-value': extract_string_value,
      'integer': extract_integer_value,
      'boolean': extract_boolean_value,
      'enum': extract_integer_value,
      'octetString': extract_bytes_value,
      'dateTime': extract_bytes_value,
      'resolution': extract_resolution_value,
      'rangeOfInteger': extract_range_value,
      'begCollection': extract_string_value,
      'endCollection': extract_string_value,
      'textWithoutLanguage': extract_string_value,
      'nameWithoutLanguage': extract_string_value,
      'keyword': extract_string_value,
      'uri': extract_string_value,
      'uriScheme': extract_string_value,
      'charset': extract_string_value,
      'naturalLanguage': extract_string_value,
      'mimeMediaType': extract_string_value,
      'memberAttrName': extract_string_value
  }
  if value_type not in function_map:
    print("value_type {} is not in function map".format(value_type))
    raise SystemExit
  return function_map[value_type](data, start, length)


# Extracts a length value from |data[start]|. It is expected that each length
# value is exactly 2 bytes. Returns a tuple containing the length extracted and
# the index to continue parsing from.
def extract_length(data, start):
  check_data_boundary(data, start, 2)
  length = (256 * data[start]) + data[start + 1]
  return (length, start + 2)


# Adds |value| to an existing IPP attribute in |group| in the IPP attributes
# dictionary |attributes|. If the existing entry is already a list, then simply
# append |value|. Otherwise, convert the existing entry to a list and append
# |value| to it.
def extend_attribute_values(attributes, group, value):
  entry = len(attributes[group]) - 1
  attribute = attributes[group][entry]
  if type(attribute['value']) is not list:
    # If the existing entry is not a list then first convert it to a list.
    list_value = [attribute['value']]
    attribute['value'] = list_value
  attribute['value'].append(value)


# Extracts the ascii values of the 2-character hex strings in |data|. If an
# element of |data| is invalid then SystemExit is raised.
def extract_ascii_values(data):
  ascii_values = []
  for val in data:
    if re.match('^[a-fA-F0-9]{2}$', val) is None:
      print('Encountered invalid byte string \'{}\''.format(val))
      raise SystemExit
    ascii_values.append(int(val, 16))
  return ascii_values


# Adds a new IPP attribute to |group| in the IPP attributes dictionary
# |attributes|.
def add_attribute(attributes, group, value_type, value_name, value):
  attributes[group].append({
      'type': value_type,
      'name': value_name,
      'value': value
  })


def main():
  if len(sys.argv) != 3:
    print("generate_ipp <input_filename> <output_filename>")
    raise SystemExit

  input_filename = sys.argv[1]
  output_filename = sys.argv[2]

  with open(input_filename) as in_file:
    data = in_file.read().split()

  ipp_attributes = {}
  current_attributes_group = None
  prev_value_type = None
  i = 0

  ascii_values = extract_ascii_values(data)

  while i < len(ascii_values):
    ascii_val = ascii_values[i]
    character = chr(ascii_val)

    if is_ipp_delimiter(ascii_val):
      if ascii_val == 3:
        break
      current_attributes_group = get_delimiter_name(ascii_val)
      ipp_attributes[current_attributes_group] = []
      attribute_entry = 0
      i += 1
    else:
      # Get the value type using the value tag |ascii_val|.
      cur_value_type = get_value_type(ascii_val)
      i += 1
      # Following the value tag, extract the name length and the name value.
      (name_length, i) = extract_length(ascii_values, i)
      (name, i) = extract_string_value(ascii_values, i, name_length)
      # Following the name info, extract the value length and the value.
      (value_length, i) = extract_length(ascii_values, i)
      (value, i) = extract_value(ascii_values, cur_value_type, i, value_length)

      if cur_value_type == prev_value_type and name_length == 0:
        # This means that the current attribute has more than one value. So we
        # must store each value in an array.
        extend_attribute_values(ipp_attributes, current_attributes_group, value)
      else:
        add_attribute(ipp_attributes, current_attributes_group, cur_value_type,
                      name, value)

      prev_value_type = cur_value_type

  if i >= len(ascii_values):
    print("Reached end of input without parsing \'endOfAttributes\' tag")
    raise SystemExit

  with open(output_filename, 'w') as output_fp:
    json.dump(ipp_attributes, output_fp, indent=True, sort_keys=False)

if __name__ == '__main__':
  main()
