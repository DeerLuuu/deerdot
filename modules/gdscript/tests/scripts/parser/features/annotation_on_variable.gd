extends Node

@tag(1)
var a := 1

@tag("x", named = 2)
var b := 2

@flag
var c := 3

func test() -> void:
	print(a, b, c)
