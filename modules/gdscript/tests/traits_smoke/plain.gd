class_name Plain

var health := 50


# 没有 `uses Health`，所以应为 false。
func takes_health() -> bool:
	return self is Health
