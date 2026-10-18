extends Node

var value_calls := 0
var right_calls := 0


func get_value() -> int:
	value_calls += 1
	return 7


func get_null() -> Variant:
	value_calls += 1
	return null


func get_right() -> int:
	right_calls += 1
	return 9


func test() -> void:
	var x := false
	var y: Variant = null
	var z := 5
	print(x or y ?? z)

	var n: Variant = null
	print(n ?? 2 + 3)

	var b: Variant = null
	var c := 5
	print(1 == (b ?? c))
	var eq: bool = 1 == (b ?? c)
	print(eq ?? false)

	var t: Variant = null
	print(t ?? 2 if true else 9)

	var a: Variant = null
	var d: Variant = null
	print(a ?? d ?? 5)

	print(get_value() ?? get_right(), value_calls, right_calls)
	print(get_null() ?? get_right(), value_calls, right_calls)
