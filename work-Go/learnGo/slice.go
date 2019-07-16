package main

import "fmt"

func main() {
	arr := [...]int{1,3,4,5,6,7}
	var s = arr[2:4]
	fmt.Println(s)

}
