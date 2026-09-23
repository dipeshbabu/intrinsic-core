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

"""Tests for quaternion validation."""

import unittest

from intrinsic.math.python import quaternion
from intrinsic.math.python import rotation3


class QuaternionTest(unittest.TestCase):

  def test_check_non_zero_rejects_values_at_or_below_custom_tolerance(self):
    for magnitude in (0.0, 0.05, 0.1):
      with self.subTest(magnitude=magnitude):
        value = quaternion.Quaternion([0, 0, 0, magnitude])

        with self.assertRaisesRegex(ValueError, "custom tolerance"):
          value.check_non_zero(norm_epsilon=0.1, err_msg="custom tolerance")

  def test_check_non_zero_accepts_values_above_custom_tolerance(self):
    for magnitude, tolerance in ((0.2, 0.1), (1e-9, 1e-10), (1e-9, 0.0)):
      with self.subTest(magnitude=magnitude, tolerance=tolerance):
        value = quaternion.Quaternion([0, 0, 0, magnitude])

        value.check_non_zero(norm_epsilon=tolerance)

  def test_check_non_zero_preserves_default_tolerance(self):
    with self.assertRaises(ValueError):
      quaternion.Quaternion([0, 0, 0, 1e-9]).check_non_zero()
    quaternion.Quaternion([0, 0, 0, 1e-7]).check_non_zero()

  def test_rotation_check_valid_honors_custom_tolerance(self):
    rotation = rotation3.Rotation3.from_xyzw([0, 0, 0, 0.05])

    with self.assertRaisesRegex(ValueError, "rotation tolerance"):
      rotation.check_valid(norm_epsilon=0.1, err_msg="rotation tolerance")


if __name__ == "__main__":
  unittest.main()
