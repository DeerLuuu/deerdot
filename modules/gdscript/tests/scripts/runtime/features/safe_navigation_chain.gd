extends Node

class Box:
	var inner: Box
	var items: Array = [1, 2]
	var value := 1

	func get_self() -> Box:
		return self

func test() -> void:
	var outer := Box.new()
	outer.inner = Box.new()
	outer.inner.value = 42
	print(outer?.inner.value)
	print(outer?.get_self().inner.value)
	print(outer?.items[1])
	print(outer?.inner?.value)
	var nil_box: Box = null
	print(nil_box?.inner.value)
	print(nil_box?.get_self())
	print(nil_box?.items[1])
