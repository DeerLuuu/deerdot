# `class_name` on the file that declares a file-level trait must not hide the
# trait from `is`: the trait is identified by the file, not by the class name.

class_name TraitWithClassName
extends Node

trait Reaches

var reach := 3

func touch() -> int:
	return reach


class User extends Node:
	uses Reaches


func test():
	var user := User.new()
	print(user is Reaches)
	print(user.touch())
	user.free()
