extends Node

signal three_changed(a, b, c)

@observable("nope")
var x := 1

@observable("three_changed")
var y := 2
