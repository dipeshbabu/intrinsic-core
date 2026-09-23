# Copyright 2026 Intrinsic Innovation LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Tests for scalar and array conversion utilities."""

import unittest

from intrinsic.math.python import math_types
import numpy as np


class MathTypesTest(unittest.TestCase):

  def test_get_matching_arrays_preserves_fractional_scalar(self):
    for values in ([1, 2, 3], np.array([[1, 2], [3, 4]]), [True, False]):
      for scalar_first in (False, True):
        with self.subTest(values=values, scalar_first=scalar_first):
          original = np.array(values, copy=True)
          inputs = (0.5, values) if scalar_first else (values, 0.5)

          lhs, rhs = math_types.get_matching_arrays(*inputs)

          expanded, array = (lhs, rhs) if scalar_first else (rhs, lhs)
          np.testing.assert_array_equal(expanded, np.full(original.shape, 0.5))
          np.testing.assert_array_equal(array, original)
          np.testing.assert_array_equal(values, original)

  def test_get_matching_arrays_preserves_scalar_with_narrow_integer_array(self):
    values = np.array([1, 2], dtype=np.int8)
    for inputs in ((values, 1000), (1000, values)):
      with self.subTest(inputs=inputs):
        lhs, rhs = math_types.get_matching_arrays(*inputs)

        expanded = rhs if inputs[0] is values else lhs
        np.testing.assert_array_equal(expanded, [1000, 1000])

  def test_get_matching_arrays_preserves_fractional_scalar_pair(self):
    for inputs in ((1, 0.5), (0.5, 1)):
      with self.subTest(inputs=inputs):
        lhs, rhs = math_types.get_matching_arrays(*inputs)

        self.assertEqual(lhs.shape, rhs.shape)
        self.assertEqual(lhs.item(), inputs[0])
        self.assertEqual(rhs.item(), inputs[1])

  def test_get_matching_arrays_rejects_mismatched_shapes(self):
    with self.assertRaisesRegex(ValueError, 'same dimension'):
      math_types.get_matching_arrays([1, 2], [1, 2, 3])


if __name__ == '__main__':
  unittest.main()
