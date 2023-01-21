import argparse
import statistics
import csv

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-f', help="trace file name", required=True)
    parser.add_argument('-o', help="output csv file name", required=True)
    parser.add_argument('-c', choices=['50', '100', '200'], help="clock freq in MHz", required=True)
    #parser.add_argument('-b', choices=['4', '8', '16'], help="burst length in words", required=True)
    args = parser.parse_args()

    #COMPENSATION = 240e-9 * 3 # TODO: check if, seems not proper

    ### Lazy filtering
    ##THRESHOLD_60_PKT = 0
    ##THRESHOLD_342_PKT_MIN = 0
    ##THRESHOLD_342_PKT_MAX = 0

    ##if int(args.b) == 4:
    ##    THRESHOLD_60_PKT = 2250 # threshold for packets greater than 60 bytes
    ##    THRESHOLD_342_PKT_MIN = 8500
    ##    THRESHOLD_342_PKT_MAX = 11000
    ##elif int(args.b) == 8:
    ##    THRESHOLD_60_PKT = 1600 # threshold for packets greater than 60 bytes
    ##    THRESHOLD_342_PKT_MIN = 6000
    ##    THRESHOLD_342_PKT_MAX = 6800
    ##elif int(args.b) == 16:
    ##    THRESHOLD_60_PKT = 1480
    ##    THRESHOLD_342_PKT_MIN = 4000
    ##    THRESHOLD_342_PKT_MAX = 5000



    file = open(args.f)
    csvfile = open(args.o, 'w')
    csvwriter = csv.writer(csvfile)

    diffs = []

    while True:
        line = file.readline()

        if not line:
            break

        if ("Actual FPGA" in line):
            #next_line = file.readline()

            #t1 = line.split()[-1]
            #t2 = next_line.split()[-1]
            #diff = float(t2) - float(t1)
            diff = int(line.split()[-1], 16)
            diff *= 1 / int(args.c) * 1000

            # compensate for bus accesses
            #compensated = diff - COMPENSATION
            #print(diff)

            diffs.append(diff)

    #diffs = [x for x in diffs if (x > THRESHOLD_60_PKT and (x < THRESHOLD_342_PKT_MIN or x > THRESHOLD_342_PKT_MAX))]
    diffs = [x for x in diffs if (x > 0)]
    mean = sum(diffs) / len(diffs)
    median = statistics.median(diffs)
    std_dev = statistics.stdev(diffs)
    min_t = min(diffs)
    max_t = max(diffs)
    print(f"Mean: {mean}")
    print(f"Std dev: {std_dev}")
    print(f"Median: {median}")
    print(f"Min: {min_t}")
    print(f"Max: {max_t}")

    csvwriter.writerows(zip(*[diffs]))

    file.close()
    csvfile.close()


if __name__ == "__main__":
    main()
