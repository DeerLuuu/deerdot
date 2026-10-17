# A trait declared in a file that has a `class_name` is still found by `uses`
# and answered by `is` from another file.

const Helper = preload("trait_class_name_helper.notest.gd")


class Lamp extends Node:
	uses Glows


func test():
	print(Helper != null)
	var lamp := Lamp.new()
	print(lamp is Glows)
	print(lamp.shine())
	lamp.free()
