extends Node

var first = null
var second = null
var third := 5

func test() -> void:
	print(first ?? second ?? third)
	print(first ?? second ?? third ?? 9)
	print(first ?? (second ?? third))
