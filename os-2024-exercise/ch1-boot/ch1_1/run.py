import os
import subprocess
import re
import struct
import binascii
import sys


def get_system_acpi_tables():
    subprocess.run(["sudo", "acpidump", "-b"])

    dat_files = [f for f in os.listdir(".") if f.endswith(".dat")]
    system_tables = {}

    for filename in dat_files:
        signature = filename[:-4].upper()
        with open(filename, "rb") as f:
            data = f.read()

        if len(data) >= 36:
            try:
                header = struct.unpack("<4sIBB6s8sI4sI", data[:36])
            except struct.error as e:
                print(f"Error unpacking header for file {filename}: {e}")
                continue

            (
                sig_in_file,
                length,
                revision,
                checksum,
                oem_id_raw,
                oem_table_id_raw,
                oem_revision,
                creator_id_raw,
                creator_revision,
            ) = header
            address = "N/A"

            try:
                oem_id = oem_id_raw.decode("ascii").rstrip("\x00").strip()
            except UnicodeDecodeError:
                oem_id = oem_id_raw.decode("latin1").rstrip("\x00").strip()
                print(
                    f"Warning: OEM ID in {filename} contains non-ASCII bytes. Decoded using 'latin1'."
                )

            checksum_hex = f"{checksum:02X}"
            length_dec = length

            system_tables[signature] = {
                "address": address,
                "length": length_dec,
                "oem_id": oem_id,
                "checksum": checksum_hex,
                "data": data,
            }
        else:
            print(f"File {filename} is too short to parse ACPI table header.")
    return system_tables


def parse_student_output(filename):
    student_tables = {}
    with open(filename, "r") as f:
        content = f.read()

    table_sections = re.split(r"(?=^\w{4} @ )", content, flags=re.MULTILINE)

    for section in table_sections:
        if not section.strip():
            continue

        header_match = re.match(r"^(\w{4}) @ (\w+)", section)
        if not header_match:
            continue

        signature = header_match.group(1)
        address = header_match.group(2)

        hex_data = ""
        hex_lines = re.findall(r"^\s*\w{4}: (.+)$", section, flags=re.MULTILINE)
        for line in hex_lines:
            hex_part = line.strip().split()
            hex_bytes = [
                byte for byte in hex_part if re.match(r"^[0-9A-Fa-f]{2}$", byte)
            ]
            hex_data += "".join(hex_bytes)

        hex_data = hex_data.strip().replace(" ", "").replace("\n", "")
        try:
            binary_data = binascii.unhexlify(hex_data)
        except binascii.Error as e:
            print(f"Error decoding hex data for table {signature}: {e}")
            continue

        if len(binary_data) >= 36:
            try:
                header = struct.unpack("<4sIBB6s8sI4sI", binary_data[:36])
            except struct.error as e:
                print(f"Error unpacking header for student table {signature}: {e}")
                continue
            (
                sig_in_file,
                length,
                revision,
                checksum,
                oem_id_raw,
                oem_table_id_raw,
                oem_revision,
                creator_id_raw,
                creator_revision,
            ) = header

            try:
                oem_id = oem_id_raw.decode("ascii").rstrip("\x00").strip()
            except UnicodeDecodeError:
                oem_id = oem_id_raw.decode("latin1").rstrip("\x00").strip()
                print(
                    f"Warning: OEM ID in student table {signature} contains non-ASCII bytes. Decoded using 'latin1'."
                )

            checksum_hex = f"{checksum:02X}"
            length_dec = length

            student_tables[signature] = {
                "address": address,
                "length": length_dec,
                "oem_id": oem_id,
                "checksum": checksum_hex,
                "data": binary_data,
            }
        else:
            print(
                f"Answer table {signature} is too short to parse ACPI table header."
            )

    return student_tables


def compare_tables(system_tables, student_tables):
    total_tables = len(system_tables)
    correct_tables = 0

    for signature, sys_info in system_tables.items():
        stud_info = student_tables.get(signature)
        if not stud_info:
            print(f"Table {signature}: Not found in Answer output.")
            continue

        address_match = (sys_info["address"] == stud_info["address"]) or sys_info[
            "address"
        ] == "N/A"
        length_match = sys_info["length"] == stud_info["length"]
        oem_id_match = sys_info["oem_id"] == stud_info["oem_id"]
        checksum_match = sys_info["checksum"].lower() == stud_info["checksum"].lower()

        content_match = sys_info["data"] == stud_info["data"]

        if address_match and length_match and oem_id_match and checksum_match:
            correct_tables += 1
            print(f"Table {signature}: Correct")
        else:
            print(f"Table {signature}: Incorrect")
            if not address_match:
                print(
                    f"\tPhysical address mismatch. System: {sys_info['address']}, Student: {stud_info['address']}"
                )
            if not length_match:
                print(
                    f"\tTable length mismatch. System: {sys_info['length']}, Student: {stud_info['length']}"
                )
            if not oem_id_match:
                print(
                    f"\tOEM ID mismatch. System: '{sys_info['oem_id']}', Student: '{stud_info['oem_id']}'"
                )
            if not checksum_match:
                print(
                    f"\tChecksum mismatch. System: {sys_info['checksum']}, Student: {stud_info['checksum']}"
                )
            if not content_match:
                print(f"\tTable content mismatch.")

    score = (correct_tables / total_tables) * 100 if total_tables > 0 else 0
    print(
        f"\nTotal Score: {score:.2f}% ({correct_tables} out of {total_tables} tables correct)"
    )


def print_system_tables(system_tables):
    for signature, info in system_tables.items():
        print(f"Table {signature} @ {info['address']}")
        print(f"  Length: {info['length']}")
        print(f"  OEM ID: '{info['oem_id']}'")
        print(f"  Checksum: {info['checksum']}\n")
        data = info["data"]
        hexdump = ""
        for i in range(0, len(data), 16):
            chunk = data[i : i + 16]
            hex_bytes = " ".join(f"{byte:02X}" for byte in chunk)
            ascii_chars = "".join(
                chr(byte) if 32 <= byte <= 126 else "." for byte in chunk
            )
            hexdump += f"{i:04X}: {hex_bytes:<47} {ascii_chars}\n"
        print(hexdump)


def main():
    print("Extracting system ACPI table information...")
    system_tables = get_system_acpi_tables()

    if len(sys.argv) > 1:
        student_output_file = sys.argv[1]
        print(f"Parsing Answer output from '{student_output_file}'...")
        student_tables = parse_student_output(student_output_file)

        print("Comparing system tables with Answer tables...")
        compare_tables(system_tables, student_tables)
    else:
        print("No student output file specified.")
        print("Printing extracted system ACPI table information...\n")
        print_system_tables(system_tables)


if __name__ == "__main__":
    main()