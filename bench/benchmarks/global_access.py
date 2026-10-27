g_counter = 0

def main():
    global g_counter
    i = 0
    while i < 5_000_000:
        g_counter = g_counter + 1
        i = i + 1
    print(g_counter)

main()
