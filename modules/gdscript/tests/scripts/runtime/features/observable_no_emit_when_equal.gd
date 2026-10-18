extends Node

@observable
var value := 1

func _init() -> void:
	var _c := value_changed.connect(func(new_value): print("changed ", new_value))
	value = 1
	value = 2
	value = 2
	print("done ", value)

func test() -> void:
	pass
