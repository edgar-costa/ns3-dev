import sys

if __name__ == "__main__":
    total = 0
    for line in sys.stdin:
        total += float(line.strip().split(" ")[0])

    print total
