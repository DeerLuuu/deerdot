extends Node

signal stats_changed(value)

@observable("stats_changed")
var hp := 1

@observable("stats_changed")
var mp := 2

var event_log := []


func test() -> void:
	var _c = stats_changed.connect(func(value): event_log.append(value))
	hp = 10
	mp = 20
	print(event_log)
