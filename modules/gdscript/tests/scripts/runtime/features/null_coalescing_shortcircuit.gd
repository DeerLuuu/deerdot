extends Node

var count := 0

func get_value() -> int:
	count += 1
	return 7

func get_right() -> int:
	count += 1
	return 9

func get_null() -> Variant:
	count += 1
	return null

func test() -> void:
	count = 0
	print(get_value() ?? get_right(), " ", count)
	count = 0
	print(get_null() ?? get_right(), " ", count)
