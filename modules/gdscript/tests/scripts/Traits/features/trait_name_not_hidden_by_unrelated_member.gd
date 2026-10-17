# A member of a class that this expression cannot name must not hide a project
# trait from `is`: only the classes and members in scope compete with it.

const HealsHelper = preload("trait_name_shadowed_helper.notest.gd")


class Other:
	const Heals := 5


class User extends Node:
	uses Heals


func test():
	# `Heals` here is the project trait, not `Other.Heals`.
	var user := User.new()
	print(user is Heals)
	print(HealsHelper != null)
	user.free()
