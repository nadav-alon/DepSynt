# General Context

## LTL

LTL (Linear Temporal Logic) is a specification language that allows one to describe a system in terms of temporal operators such as Until, Globaly, Future, Next, etc.

## LTL Synthesis

LTL Synthesis is the problem in which you get an LTL formula and a partition of the variables to inputs and outputs, with the result being a strategy that allows you set the output values in response to the input values you recieve, that satisfies the LTL formula.

## Output Dependencies

Output Dependencies are output variables in LTL formulas that are uniquely determined by other variables. That means that if those variables are set not as determined by other variables, the formula is violated immediately. This repo contains an implementation of a method to find output dependencies using compatible states in the automaton of the formula, and a method to simplify the synthesis problem by extracting them from the formula and solving them independently, solving the remaining problem, then merging the strategies.

## Input Dependencies

Input dependencies are a new concept that we are exploring, in which we want to find input variables that are dependencies. Our definition of input dependencies is the inverse of output dependencies, in the sense that if those variables are not set as determined by other variables, the formula is satisfied immediately. That way the strategy can assume those input variables will be set a certain way in its strategy, otherwise it wins.