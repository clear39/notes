package main

import "fmt"

func main() {
	fmt.Println("hello world")

	var arr0 [6]int

	for i,_ := range arr0 {
		arr0[i] = i*2 + 1
	}
	fmt.Println(arr0)

	var arr1  =  []int{1,2,3,4,5,6}
	fmt.Println(arr1)

	var arr2 = [3]int{2,4,6}
	fmt.Println(arr2)


	var arr3 = [...]int{2,4,6}
	arr4 := [...]int{2,4,6}

	fmt.Println(arr3)

	fmt.Println(arr4)

	fmt.Println(arr3[:2])

}
