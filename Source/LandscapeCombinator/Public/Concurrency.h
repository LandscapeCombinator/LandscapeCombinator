#pragma once

class Concurrency {
public:
	static void RunAsync(TFunction<void()> Action);
};
