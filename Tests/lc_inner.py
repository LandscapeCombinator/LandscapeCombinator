import sys, unreal

n = 0
try:
    unreal.EditorLoadingAndSavingUtils.load_map(sys.argv[1])
    # temporary, in memory only: use a local Overpass server instead of the one saved in the assets
    url = "http://localhost:12345/api/interpreter"
    for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
        try:
            a.get_editor_property("overpass_server")
            a.set_editor_property("overpass_server", url)
        except Exception:
            pass
    tags = sys.argv[2].split(",")
    actors = [a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors() if any(str(t) in tags for t in a.tags)]
    for a in actors:
        print(f"LCTEST_TRIGGER {a.get_actor_label()}", flush=True)
        a.generate_actors_no_prompt()
        n += 1
    print(f"LCTEST_DONE {n}")
except Exception as e:
    unreal.log_error(f"Exception: {e!r}")
