extends Node

class Thing:
	var value := 7
	var inner: Thing

	func method() -> int:
		return 9

var evaluations := 0

func get_thing() -> Thing:
	evaluations += 1
	return Thing.new()

func get_null_thing() -> Thing:
	evaluations += 1
	return null

func test() -> void:
	var nil_thing: Thing = null
	print(nil_thing?.value)
	print(nil_thing?.method())
	var thing := Thing.new()
	print(thing?.value)
	print(thing?.method())
	evaluations = 0
	print(get_thing()?.value, " ", evaluations)
	evaluations = 0
	print(get_null_thing()?.value, " ", evaluations)
	print(nil_thing?.inner.value)
	var typed: int = get_thing()?.value
	print(typed)
