extends Node

signal hp_changed(new_hp)
signal auto_signal(value)

@observable("hp_changed")
var hp := 10

@observable
var automatic := 1

var event_log := []


func test() -> void:
	var _c1 = hp_changed.connect(func(new_hp): event_log.append(["hp", new_hp]))
	var _c2 = auto_signal.connect(func(value): event_log.append(["b", value]))
	var _c3 = automatic_changed.connect(func(value): event_log.append(["auto", value]))
	hp = 20
	hp = 20
	automatic = 2
	hp = 30
	print(event_log)
	var script: GDScript = get_script()
	print(script.has_annotation(&"hp", &"observable"))
