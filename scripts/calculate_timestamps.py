import argparse
import statistics
import csv

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-f', help="trace file name", required=True)
    parser.add_argument('-o', help="output csv file name", required=True)
    args = parser.parse_args()

    COMPENSATION = 240e-9 * 2

    file = open(args.f)
    csvfile = open(args.o, 'w')
    csvwriter = csv.writer(csvfile)

    total = 0
    diffs = []

    while True:
        line = file.readline()

        if not line:
            break

        if ("Before XDP" in line):
            next_line = file.readline()

            t1 = line.split()[3][:-1]
            t2 = next_line.split()[3][:-1]
            diff = float(t2) - float(t1)

            # compensate for bus accesses
            compensated = diff - COMPENSATION
            #print(compensated)

            diffs.append(compensated)
            total += 1

    mean = sum(diffs) / total
    std_dev = statistics.stdev(diffs)
    print(f"Mean: {mean}")
    print(f"Std dev: {std_dev}")

    csvwriter.writerows(zip(*[diffs]))

    file.close()
    csvfile.close()


if __name__ == "__main__":
    main()
