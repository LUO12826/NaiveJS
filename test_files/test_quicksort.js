const arr = [254, 1320, 1398, 255]
quickSort(arr)

console.log(arr)

function partition(arr, low, high) {
    let i = low, j = high, pivot = arr[i];
    while (i < j) {
        while (j > i && arr[j] >= pivot) {
            j -= 1
        }
        arr[i] = arr[j]

        while (i < j && arr[i] <= pivot) {
            i += 1
        }
        arr[j] = arr[i]
    }
    arr[i] = pivot
    return i
}

function sort(arr, low, high) {
    if (low < high) {
        const pivot = partition(arr, low, high)
        if (pivot - 1 >= low) sort(arr, low, pivot - 1)
        if (pivot + 1 <= high) sort(arr, pivot + 1, high)
    }
}

function quickSort(arr) {
    sort(arr, 0, arr.length - 1)
}
