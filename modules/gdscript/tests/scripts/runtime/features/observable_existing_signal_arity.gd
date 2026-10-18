extends Node

signal zero_changed()
signal one_changed(value)
signal two_changed(old_value, new_value)

@observable("zero_changed")
var z := 0

@observable("one_changed")
var o := 0

@observable("two_changed")
var t := 0

var event_log := []


func test() -> void:
	var _c1 = zero_changed.connect(func(): event_log.append("zero"))
	var _c2 = one_changed.connect(func(value): event_log.append(["one", value]))
	var _c3 = two_changed.connect(func(old_value, new_value): event_log.append(["two", old_value, new_value]))
	z = 1
	o = 5
	t = 7
	print(event_log)
