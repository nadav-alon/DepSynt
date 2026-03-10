import sys
import os
import unittest

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
from input_dep_report import add_turn_delay

class TestAddTurnDelay(unittest.TestCase):

    def test_add_turn_delay_basic(self):
        formula = "G(i1 -> o1) & G(i2 -> o2)"
        variables = "i1,i2"
        expected = "G((X i1) -> o1) & G((X i2) -> o2)"
        self.assertEqual(add_turn_delay(formula, variables), expected)

    def test_add_turn_delay_spaces(self):
        formula = "G(i1 -> o1) & G(i2 -> o2)"
        variables = "i1, i2 "
        expected = "G((X i1) -> o1) & G((X i2) -> o2)"
        self.assertEqual(add_turn_delay(formula, variables), expected)

    def test_add_turn_delay_substrings(self):
        formula = "G(i1 -> i11) & G(new_i1 -> o1)"
        variables = "i1"
        expected = "G((X i1) -> i11) & G(new_i1 -> o1)"
        self.assertEqual(add_turn_delay(formula, variables), expected)

    def test_add_turn_delay_empty_var(self):
        formula = "G(i1 -> o1)"
        variables = "i1, "
        expected = "G((X i1) -> o1)"
        self.assertEqual(add_turn_delay(formula, variables), expected)

    def test_add_turn_delay_multiple_occurrences(self):
        formula = "G(i1 & i1) | F(i1)"
        variables = "i1"
        expected = "G((X i1) & (X i1)) | F((X i1))"
        self.assertEqual(add_turn_delay(formula, variables), expected)

    def test_add_turn_delay_no_variables(self):
        formula = "G(o1 -> o2)"
        variables = ""
        expected = "G(o1 -> o2)"
        self.assertEqual(add_turn_delay(formula, variables), expected)

if __name__ == "__main__":
    unittest.main()
